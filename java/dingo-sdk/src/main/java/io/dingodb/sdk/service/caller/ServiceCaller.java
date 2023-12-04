package io.dingodb.sdk.service.caller;

import io.dingodb.sdk.common.DingoClientException.InvalidRouteTableException;
import io.dingodb.sdk.common.DingoClientException.RequestErrorException;
import io.dingodb.sdk.common.DingoClientException.RetryException;
import io.dingodb.sdk.common.utils.ErrorCodeUtils;
import io.dingodb.sdk.common.utils.ReflectionUtils;
import io.dingodb.sdk.service.Caller;
import io.dingodb.sdk.service.ChannelProvider;
import io.dingodb.sdk.service.Service;
import io.dingodb.sdk.service.entity.Message;
import io.dingodb.sdk.service.entity.error.Error;
import io.grpc.CallOptions;
import io.grpc.Channel;
import io.grpc.MethodDescriptor;
import lombok.SneakyThrows;
import lombok.extern.slf4j.Slf4j;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.locks.LockSupport;
import java.util.function.Supplier;

import static io.dingodb.sdk.common.utils.ErrorCodeUtils.Strategy.FAILED;
import static io.dingodb.sdk.common.utils.ErrorCodeUtils.Strategy.IGNORE;
import static io.dingodb.sdk.common.utils.ErrorCodeUtils.Strategy.REFRESH;
import static io.dingodb.sdk.common.utils.ErrorCodeUtils.Strategy.RETRY;
import static io.dingodb.sdk.common.utils.ErrorCodeUtils.errorToStrategy;

@Slf4j
public class ServiceCaller<S extends Service<S>> implements InvocationHandler, Caller<S> {

    private static final ThreadLocal<Map<String, Integer>> ERR_MSGS = ThreadLocal.withInitial(HashMap::new);

    private int retry;
    private CallOptions options;

    private final ChannelProvider channelProvider;

    private final Class<S> genericType;
    private final S service;

    private final Set<MethodDescriptor> directCallOnce = new HashSet<>();

    public ServiceCaller(ChannelProvider channelProvider, int retry, CallOptions options) {
        this.channelProvider = channelProvider;
        this.retry = retry;
        this.options = options;
        this.genericType = ReflectionUtils.getGenericType(this.getClass(), 0);

        service = proxy(genericType);
    }

    public ServiceCaller<S> addExcludeErrorCheck(MethodDescriptor method) {
        directCallOnce.add(method);
        return this;
    }

    private S proxy(Class<S> genericType) {
        for (Class<?> child : genericType.getClasses()) {
            if (!child.getSuperclass().equals(genericType)) {
                try {
                    return (S) child.getConstructor(Caller.class).newInstance(this);
                } catch (Exception e) {
                    throw new RuntimeException(e);
                }
            }
        }
        throw new RuntimeException("Not found " + genericType.getName() + " impl.");
    }

    public int retry() {
        return retry;
    }

    public ServiceCaller<S> retry(int retry) {
        this.retry = retry;
        return this;
    }

    public CallOptions options() {
        return options;
    }

    public ServiceCaller<S> options(CallOptions options) {
        this.options = options;
        return this;
    }

    @Override
    public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
        return method.invoke(service, args);
    }

    @Override
    @SneakyThrows
    public <REQ extends Message, RES extends Message.Response> RES call(
        MethodDescriptor<REQ, RES> method, Supplier<REQ> provider
    ) {
        return call(method, provider, System.identityHashCode(provider));
    }

    @Override
    public <REQ extends Message, RES extends Message.Response> RES call(
        MethodDescriptor<REQ, RES> method, REQ request
    ) {
        return call(method, (Supplier<REQ>) () -> request);
    }

    public <REQ extends Message, RES extends Message.Response> RES call(
        MethodDescriptor<REQ, RES> method, Supplier<REQ> provider, long traceId
    ) throws Exception {

        SimpleChannelProvider channelProvider = new SimpleChannelProvider(this.channelProvider.channel());
        int retry = this.retry;
        boolean connected = false;
        ERR_MSGS.get().clear();
        String methodName = method.getFullMethodName();
        while (retry-- > 0) {
            try {
                Channel channel = channelProvider.getChannel();
                REQ request = provider.get();
                if (log.isDebugEnabled()) {
                    log.debug("Invoke [{}] begin, trace [{}], request: {}, options: {}", methodName, traceId,
                        request, options
                    );
                }
                RES response = RpcCaller.asyncCall(method, request, options, channel, traceId).join();
                if (response == null) {
                    if (log.isDebugEnabled()) {
                        log.debug(
                            "Invoke [{}] return null, refresh and wait retry, trace [{}], request: {}, options: {}",
                            methodName, traceId, request, options
                        );
                    }
                    updateChannel(channelProvider, traceId);
                    continue;
                }
                connected = true;
                if (response.getError() != null && response.getError().getErrcode().number() != 0) {
                    Error error = response.getError();
                    int errCode = error.getErrcode().number();
                    ERR_MSGS.get().compute(
                        channel.authority() + ">>" + error.getErrmsg(), (k, v) -> v == null ? 1 : v + 1
                    );
                    switch (errorToStrategy(errCode)) {
                        case RETRY:
                            errorLog(methodName, channel.authority(), traceId, error, RETRY);
                            updateChannel(channelProvider, traceId);
                            continue;
                        case FAILED:
                            errorLog(methodName, channel.authority(), traceId, error, FAILED);
                            throw new RequestErrorException(errCode, error.getErrmsg());
                        case REFRESH:
                            errorLog(methodName, channel.authority(), traceId, error, REFRESH);
                            updateChannel(channelProvider, traceId);
                            throw new InvalidRouteTableException(error.getErrmsg());
                        case IGNORE:
                            errorLog(methodName, channel.authority(), traceId, error, IGNORE);
                            return null;
                        default:
                            throw new IllegalStateException("Unexpected value: " + errorToStrategy(errCode));
                    }
                }
                if (log.isDebugEnabled()) {
                    log.debug("Invoke [{}] finish, trace [{}], request: {}, options: {}, response: {}",
                        methodName, traceId, request, options, response
                    );
                }
                return response;
            } catch (Exception e) {
                if (e instanceof RequestErrorException || e instanceof InvalidRouteTableException) {
                    throw e;
                }
                if (log.isDebugEnabled()) {
                    log.warn("Exec {} failed: {}.", methodName, e.getMessage());
                }
                ERR_MSGS.get().compute(e.getMessage(), (k, v) -> v == null ? 1 : v + 1);
                waitRetry();
            }
        }

        throw generateException(methodName, traceId, connected);
    }

    private void waitRetry() {
        LockSupport.parkNanos(TimeUnit.SECONDS.toNanos(1));
    }

    private void updateChannel(SimpleChannelProvider channelProvider, long trace) {
        this.channelProvider.refresh(channelProvider.getChannel(), trace);
        waitRetry();
        channelProvider.setChannel(this.channelProvider.channel());
    }

    private RuntimeException generateException(String name, long traceId, boolean connected) {
        // if connected is false, means can not get leader connection
        if (connected) {
            StringBuilder errMsgBuilder = new StringBuilder();
            errMsgBuilder.append("task: ").append(name).append(", trace: ").append(traceId).append(" ==>> ");
            ERR_MSGS.get().forEach(
                (k, v) -> errMsgBuilder.append('[').append(v).append("] times [").append(k).append(']').append(", ")
            );
            throw new RetryException("Exec attempts exhausted, failed to exec " + name + ", " + errMsgBuilder);
        } else {
            throw new RetryException(
                "Exec [" + traceId + "] [" + name + "] error, " + "transform leader attempts exhausted."
            );
        }
    }

    private void errorLog(
        String name, String remote, long traceId, Error error, ErrorCodeUtils.Strategy strategy
    ) {
        if (log.isDebugEnabled()) {
            log.warn(
                "Exec {} failed, remote: [{}] trace: [{}], code: [{}], message: {}, strategy: {}.",
                name, remote, traceId, error.getErrmsg(), error.getErrmsg(), strategy
            );
        }
    }

}