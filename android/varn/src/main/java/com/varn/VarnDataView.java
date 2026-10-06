package com.varn;

import java.math.BigInteger;
import java.nio.ByteBuffer;
import org.json.JSONException;
import org.json.JSONObject;

/**
 * A reference to memory that Java, the native code and Lua share without copying it, such as a camera frame.
 *
 * <p>Each object owns one reference, which {@link #close()} gives back, and the memory is freed or handed back once the last reference of every language ends. The native library that holds the runtime is loaded before the first view is made.
 */
public final class VarnDataView implements AutoCloseable {
    private long handle;
    private ByteBuffer buffer;

    private VarnDataView(long handle) {
        this.handle = handle;
    }

    /** Allocates a view of zeroed bytes. */
    public static VarnDataView allocate(long size) {
        if (size < 0) {
            throw new IllegalArgumentException("A data view needs a size of zero or more bytes.");
        }
        long created = nativeAllocate(size);
        if (created == 0) {
            throw new OutOfMemoryError("A data view of " + size + " bytes could not be allocated.");
        }
        return new VarnDataView(created);
    }

    /** Shares the memory of a direct buffer without copying it, keeping the buffer alive until the last reference ends. */
    public static VarnDataView wrap(ByteBuffer direct) {
        if (direct == null || !direct.isDirect()) {
            throw new IllegalArgumentException("A data view wraps only a direct ByteBuffer.");
        }
        return new VarnDataView(nativeWrap(direct));
    }

    /** Takes a reference to the live view that a token names, or answers null when that view is gone. */
    public static VarnDataView acquire(JSONObject token) throws JSONException {
        String pointer = token.getString("pointer");
        if (token.length() != 3 || !pointer.startsWith("0x")) {
            throw new JSONException("A data view token holds exactly the id \"$view\", the pointer as a hexadecimal string and the size.");
        }
        long acquired = nativeAcquire(token.getLong("$view"), new BigInteger(pointer.substring(2), 16).longValue(), token.getLong("size"));
        return acquired != 0 ? new VarnDataView(acquired) : null;
    }

    /** Whether a value of parsed JSON is the token of a view. */
    public static boolean isToken(Object value) {
        return value instanceof JSONObject && ((JSONObject) value).has("$view");
    }

    /** Takes over a reference that native code hands to Java, such as a view of a call that a host passes to its handler. */
    public static VarnDataView adopt(long handle) {
        if (handle == 0) {
            throw new IllegalArgumentException("A data view needs a native handle.");
        }
        return new VarnDataView(handle);
    }

    /** A direct buffer over the memory of the view, valid until this object is closed. */
    public synchronized ByteBuffer buffer() {
        long current = checkedHandle();
        if (buffer == null) {
            buffer = nativeBuffer(current);
        }
        return buffer.duplicate();
    }

    public long size() {
        return nativeSize(checkedHandle());
    }

    public long id() {
        return nativeId(checkedHandle());
    }

    public long pointer() {
        return nativePointer(checkedHandle());
    }

    /** The native handle of this reference, for native code that takes its own reference with {@code varn_dataview_retain}. */
    public long handle() {
        return checkedHandle();
    }

    /** The JSON token that names the view, which owns nothing, so the sender keeps the view alive until the call that carries it returns. */
    public JSONObject token() {
        long current = checkedHandle();
        try {
            return new JSONObject().put("$view", nativeId(current)).put("pointer", "0x" + Long.toHexString(nativePointer(current))).put("size", nativeSize(current));
        } catch (JSONException error) {
            throw new IllegalStateException("The token of a data view could not be written.", error);
        }
    }

    /** Another reference to the same view, which its own {@link #close()} gives back. */
    public VarnDataView retain() {
        long current = checkedHandle();
        nativeRetain(current);
        return new VarnDataView(current);
    }

    public synchronized boolean isClosed() {
        return handle == 0;
    }

    /** Gives the reference back, after which this object refuses every use, and closing again does nothing. */
    @Override
    public synchronized void close() {
        if (handle != 0) {
            nativeRelease(handle);
            handle = 0;
            buffer = null;
        }
    }

    private synchronized long checkedHandle() {
        if (handle == 0) {
            throw new IllegalStateException("The data view was closed.");
        }
        return handle;
    }

    private static native long nativeAllocate(long size);
    private static native long nativeWrap(ByteBuffer direct);
    private static native long nativeAcquire(long id, long pointer, long size);
    private static native void nativeRetain(long handle);
    private static native void nativeRelease(long handle);
    private static native ByteBuffer nativeBuffer(long handle);
    private static native long nativeSize(long handle);
    private static native long nativeId(long handle);
    private static native long nativePointer(long handle);
}
