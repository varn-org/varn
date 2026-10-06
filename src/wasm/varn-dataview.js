// A reference to memory of the WebAssembly heap that JavaScript, the native code and Lua share without copying it, such as the pixels of a video frame.
// Each object owns one reference, which `release` gives back, and the memory is freed once the last reference of every language ends.
// A build that links this file exports the functions "_varn_dataview_new", "_varn_dataview_acquire", "_varn_dataview_retain", "_varn_dataview_release", "_varn_dataview_data", "_varn_dataview_size" and "_varn_dataview_id" and the view "HEAPU8".
class VarnDataView {
  #handle;

  constructor(handle) {
    this.#handle = handle;
  }

  // Allocates a view of zeroed bytes.
  static allocate(size) {
    if (!Number.isInteger(size) || size < 0) {
      throw new RangeError("A data view needs a size of zero or more bytes.");
    }
    const handle = _varn_dataview_new(size);
    if (handle === 0) {
      throw new RangeError(`A data view of ${size} bytes could not be allocated.`);
    }
    return new VarnDataView(handle);
  }

  // Takes a reference to the live view that a token names, or answers null when that view is gone.
  static acquire(token) {
    if (!VarnDataView.isToken(token) || Object.keys(token).length !== 3 || typeof token.pointer !== "string" || !token.pointer.startsWith("0x")) {
      throw new TypeError('A data view token holds exactly the id "$view", the pointer as a hexadecimal string and the size.');
    }
    const handle = _varn_dataview_acquire(BigInt(token.$view), Number.parseInt(token.pointer.slice(2), 16), token.size);
    return handle !== 0 ? new VarnDataView(handle) : null;
  }

  // Whether a value of parsed JSON is the token of a view.
  static isToken(value) {
    return value !== null && typeof value === "object" && Object.hasOwn(value, "$view");
  }

  // Takes over a reference that native code hands to JavaScript.
  static adopt(handle) {
    return new VarnDataView(handle);
  }

  get handle() {
    return this.#checked();
  }

  // Addresses and sizes of a 32-bit heap are unsigned, while WebAssembly hands them over as signed integers.
  get pointer() {
    return _varn_dataview_data(this.#checked()) >>> 0;
  }

  get size() {
    return _varn_dataview_size(this.#checked()) >>> 0;
  }

  get id() {
    return Number(_varn_dataview_id(this.#checked()));
  }

  get released() {
    return this.#handle === 0;
  }

  // The bytes of the view on the heap as it is now, which a growth of the heap detaches, so a caller asks again after anything that may allocate.
  bytes() {
    const pointer = this.pointer;
    return HEAPU8.subarray(pointer, pointer + this.size);
  }

  // The JSON token that names the view, which owns nothing, so the sender keeps the view alive until the call that carries it returns.
  token() {
    return { $view: this.id, pointer: `0x${this.pointer.toString(16)}`, size: this.size };
  }

  // Another reference to the same view, which its own `release` gives back.
  retain() {
    const handle = this.#checked();
    _varn_dataview_retain(handle);
    return new VarnDataView(handle);
  }

  // Gives the reference back, after which this object refuses every use, and releasing again does nothing.
  release() {
    if (this.#handle !== 0) {
      _varn_dataview_release(this.#handle);
      this.#handle = 0;
    }
  }

  #checked() {
    if (this.#handle === 0) {
      throw new Error("The data view was released.");
    }
    return this.#handle;
  }
}

Module["VarnDataView"] = VarnDataView;
