export type Example = { module: string; label: string; code: string };

export const DEFAULT_LUA = `local function fib(n)
  if n < 2 then
    return n
  end
  return fib(n - 1) + fib(n - 2)
end

print("fib(12) =", fib(12))

for i = 1, 4 do
  print(string.format("loop %d", i))
end
`;

// The examples are grouped by module so the playground mirrors what the browser build carries.
// The modules `socket`, `process` and `ffi` and the HTTP server are native-only, and the example of `platform` that asks what this build can do shows the error each one raises here.
// The camera and the page itself reach Lua through the functions the worker registers under the global table `host`.
export const EXAMPLES: ReadonlyArray<Example> = [
  { module: "Lua", label: "Lua — fibonacci", code: DEFAULT_LUA },
  {
    module: "Lua",
    label: "Lua — strings & patterns",
    code: `local text = "varn: fast, small, embeddable"

-- A pattern splits the sentence into words.
for word in text:gmatch("%a+") do
  print(word)
end

print("upper:", text:upper())
print("commas:", select(2, text:gsub(",", "")))
`,
  },
  {
    module: "Lua",
    label: "Lua — tables & iteration",
    code: `local fruits = { "apple", "banana", "cherry" }

-- An array iterates in order.
for index, name in ipairs(fruits) do
  print(index, name)
end

local counts = { apple = 3, banana = 5 }
counts.cherry = 1

-- A map iterates by key.
for name, n in pairs(counts) do
  print(name, "=", n)
end
`,
  },
  {
    module: "Lua",
    label: "Lua — error handling",
    code: `-- The function \`pcall\` turns a runtime error into a value you can inspect.
local ok, err = pcall(function()
  error("Something went wrong.")
end)
print("ok:", ok)
print("err:", err)

local divided, result = pcall(function(a, b)
  return a / b
end, 10, 2)
print("divided:", divided, "result:", result)
`,
  },
  {
    module: "Lua stdlib",
    label: "string — format, find, gsub, bytes",
    code: `-- The string library covers formatting, search, substitution and bytes.
local s = "Varn 1.0 fast and small"
print("format:", string.format("%s has %d chars", s, #s))
print("find:", s:find("%d+%.%d+"))
print("gsub:", (s:gsub("%s+", "_")))
print("upper sub rep:", s:upper():sub(1, 4):rep(2))

local words = {}
for word in s:gmatch("%a+") do
  words[#words + 1] = word
end
print("words:", table.concat(words, " "))
print("byte and char:", string.byte("A"), string.char(86, 97, 114, 110))
`,
  },
  {
    module: "Lua stdlib",
    label: "table — insert, sort, concat, unpack",
    code: `-- The table library builds, sorts, joins and unpacks arrays.
local t = { "banana", "apple", "cherry" }
table.insert(t, "date")
table.remove(t, 1)
table.sort(t)
print("sorted:", table.concat(t, ", "))

local nums = { 3, 1, 2 }
table.sort(nums, function(a, b) return a > b end)
print("desc:", table.unpack(nums))
print("count:", #t)
`,
  },
  {
    module: "Lua stdlib",
    label: "math — round, roots, random",
    code: `-- The math library covers rounding, roots, extremes and randomness.
print("floor and ceil:", math.floor(3.7), math.ceil(3.2))
print("sqrt and abs:", math.sqrt(144), math.abs(-9))
print("max and min:", math.max(3, 8, 1), math.min(3, 8, 1))
print("pi:", string.format("%.5f", math.pi))

math.randomseed(42)
print("random:", math.random(1, 6), math.random(1, 6))
print("type:", math.type(3), math.type(3.0))
`,
  },
  {
    module: "Lua stdlib",
    label: "coroutine — cooperative tasks",
    code: `-- Coroutines are cooperative tasks that yield values and resume where they left off.
local function squares(limit)
  for i = 1, limit do
    coroutine.yield(i * i)
  end
end

local co = coroutine.create(squares)
for _ = 1, 3 do
  local ok, value = coroutine.resume(co, 3)
  print("resumed:", ok, value)
end
print("status:", coroutine.status(co))

local gen = coroutine.wrap(function()
  for c in ("abc"):gmatch(".") do
    coroutine.yield(c)
  end
end)
print("wrap:", gen(), gen(), gen())
`,
  },
  {
    module: "Lua stdlib",
    label: "metatable — operators & objects",
    code: `-- Metatables give operator overloading and prototype-based objects.
local Vec = {}
Vec.__index = Vec

function Vec.new(x, y)
  return setmetatable({ x = x, y = y }, Vec)
end

function Vec.__add(a, b)
  return Vec.new(a.x + b.x, a.y + b.y)
end

function Vec:length()
  return math.sqrt(self.x ^ 2 + self.y ^ 2)
end

function Vec.__tostring(v)
  return string.format("(%d, %d)", v.x, v.y)
end

local sum = Vec.new(3, 4) + Vec.new(1, 2)
print("sum:", tostring(sum))
print("length:", Vec.new(3, 4):length())
`,
  },
  {
    module: "Lua stdlib",
    label: "os — time, date, clock",
    code: `-- The os library covers time, dates and a high-resolution clock.
print("time is a number:", type(os.time()))
print("formatted date:", os.date("!%Y-%m-%d %H:%M:%S", 1750000000))

local started = os.clock()
local sum = 0
for i = 1, 1000000 do
  sum = sum + i
end
print("sum 1..1e6:", sum)
print(string.format("cpu time: %.3f s", os.clock() - started))
`,
  },
  {
    module: "Lua stdlib",
    label: "string.pack & utf8 — binary records and text",
    code: `-- The function \`string.pack\` lays values out as bytes, the way a file format or a network protocol does.
local layout = "<I4 i2 d s1"
local record = string.pack(layout, 1042, -7, math.pi, "Ada")
local hex = record:gsub(".", function(byte)
  return string.format("%02x ", byte:byte())
end)
print(string.format("%d bytes: %s", #record, hex))
print("Unpacked:", string.unpack(layout, record))

-- Integers and bitwise operators work on 64 bits, here packing a color into one number and taking it apart.
local color = 0x3a << 16 | 0xb5 << 8 | 0xe6
print(string.format("Color 0x%06x has red %d, green %d and blue %d.", color, color >> 16, color >> 8 & 0xff, color & 0xff))
print("Floor division and remainder:", 17 // 5, 17 % 5, -17 // 5, math.maxinteger + 1 == math.mininteger)

-- The library \`utf8\` counts characters rather than bytes, and walks the code points of a text.
local word = "São Paulo ☀"
print(string.format("%q takes %d bytes and holds %d characters.", word, #word, utf8.len(word)))
local points = {}
for _, code in utf8.codes(word) do
  points[#points + 1] = string.format("U+%04X", code)
end
print(table.concat(points, " "))
print("Rebuilt from code points: " .. utf8.char(0x56, 0x61, 0x72, 0x6E, 0x20, 0x2764))
`,
  },
  {
    module: "async",
    label: "async — sleep & spawn",
    code: `local async = require("async")

print("Start.")
async.spawn(function()
  print("Task started.")
  async.sleep(1500):await()
  print("Task woke up after 1500ms.")
end)
print("The main chunk returned first.")
`,
  },
  {
    module: "async",
    label: "async — all, race, any & timeout",
    code: `local async = require("async")
local datetime = require("datetime")

local started = datetime.now():millis()

local function stamp(text)
  print(string.format("%5d ms  %s", datetime.now():millis() - started, text))
end

-- A pretend download that takes as long as it is told and then answers its name, or fails.
local function fetch(name, ms, fails)
  return async.promise(function()
    async.sleep(ms):await()
    if fails then
      error("The mirror " .. name .. " failed.", 0)
    end
    return name
  end)
end

async.spawn(function()
  stamp("All three mirrors at once, so the slowest sets the pace.")
  local all = async.all({ fetch("north", 300), fetch("south", 600), fetch("east", 450) }):await()
  stamp("All answered " .. table.concat(all, ", ") .. ".")

  stamp("A race, where the first to settle wins.")
  stamp("The race answered " .. async.race({ fetch("slow", 500), fetch("fast", 150) }):await() .. ".")

  stamp("Any success wins, and a failure is skipped.")
  stamp("Any answered " .. async.any({ fetch("broken", 50, true), fetch("steady", 250) }):await() .. ".")

  stamp("Every outcome, success or failure.")
  local outcomes = async.allSettled({ fetch("first", 100), fetch("second", 120, true) }):await()
  for i, outcome in ipairs(outcomes) do
    stamp(string.format("  Outcome %d: %s", i, outcome.ok and outcome.value or outcome.error))
  end

  stamp("A deadline for what takes too long.")
  local value, err = async.timeout(fetch("glacial", 2000), 400):await()
  stamp("The deadline answered " .. tostring(value) .. ": " .. err)
end)
`,
  },
  {
    module: "async",
    label: "async — mapLimit, a pool of three",
    code: `local async = require("async")
local datetime = require("datetime")

local started = datetime.now():millis()
local inFlight, peak = 0, 0

-- Twelve pictures to render, but only three render at a time, the way a pool of connections or workers is shared.
local jobs = {}
for i = 1, 12 do
  jobs[i] = { id = i, ms = 120 + (i * 53) % 260 }
end

async.spawn(function()
  local rendered = async.mapLimit(jobs, 3, function(job)
    return async.promise(function()
      inFlight = inFlight + 1
      peak = math.max(peak, inFlight)
      print(string.format("%5d ms  Picture %2d starts  %s", datetime.now():millis() - started, job.id, string.rep("#", inFlight)))
      async.sleep(job.ms):await()
      inFlight = inFlight - 1
      return "picture-" .. job.id
    end)
  end):await()

  print(string.format("Rendered %d pictures in %d ms, with at most %d at a time.", #rendered, datetime.now():millis() - started, peak))
  print("The results keep the order of the input: " .. table.concat(rendered, ", ", 1, 4) .. ", …")
end)
`,
  },
  {
    module: "async",
    label: "async — deferred, cancel & failures",
    code: `local async = require("async")

-- One handler receives every failure that nothing awaits, so no failure passes silently.
async.onFailure(function(err, traceback, frames, kind)
  print(string.format("The failure handler received a %s: %s", kind, tostring(err)))
end)

async.spawn(function()
  -- A deferred is a promise that something else settles, here a button press that a timer stands in for.
  local pressed, press = async.deferred()
  async.spawn(function()
    async.sleep(300):await()
    print("The button was pressed.")
    press()
  end)
  print("Waiting for the button.")
  print("The deferred settled with " .. tostring(pressed:await()) .. ".")

  -- A cancelled task stops at its next await and never runs the rest.
  local download = async.spawn(function()
    for percent = 0, 100, 20 do
      print("Downloading, " .. percent .. "% done.")
      async.sleep(100):await()
    end
    print("This line never prints.")
  end)
  async.sleep(250):await()
  download.cancel()
  print("The download was cancelled.")

  -- A rejection can be caught and turned into a value.
  local page = async.promise(function()
    error("The server is down.", 0)
  end):catch(function(err)
    return "the cached copy, since " .. err:lower()
  end):await()
  print("Showing " .. page)

  -- A failing task and a promise that nothing observes both reach the handler.
  async.spawn(function()
    error("A task broke.", 0)
  end)
  async.promise(function()
    error("A promise was forgotten.", 0)
  end)
  async.sleep(50):await()

  -- The handler stays for the whole page, so it is cleared before the next run.
  async.onFailure(nil)
end)
`,
  },
  {
    module: "async",
    label: "async — yield inside long work",
    code: `local async = require("async")
local datetime = require("datetime")

local started = datetime.now():millis()
local finished = false

local function stamp(text)
  print(string.format("%5d ms  %s", datetime.now():millis() - started, text))
end

-- A clock that ticks while the heavy work below runs, which it can only do because that work yields.
async.spawn(function()
  local ticks = 0
  while not finished do
    ticks = ticks + 1
    stamp("Tick " .. ticks .. ".")
    async.sleep(20):await()
  end
end)

-- Random points estimate pi, and the work hands the loop back after every slice of points.
async.spawn(function()
  local inside, thrown, slice = 0, 0, 400000

  for _ = 1, 15 do
    for _ = 1, slice do
      local x, y = math.random(), math.random()
      if x * x + y * y <= 1 then
        inside = inside + 1
      end
    end

    thrown = thrown + slice
    stamp(string.format("After %d points, pi is about %.5f.", thrown, 4 * inside / thrown))
    async.yield():await()
  end

  finished = true
end)
`,
  },
  {
    module: "dataview",
    label: "dataview — webcam in black & white",
    code: `local async = require("async")
local datetime = require("datetime")

-- The page opens the camera with the permission prompt of the browser and writes each frame into one data view of RGBA bytes that Lua and the page share.
local width, height = 320, 240
local opened = host.cameraStart({ width = width, height = height })
if opened.error then
  error(opened.error)
end

-- Each pixel takes its luma in place, so the page draws the very bytes Lua changed and nothing is copied between them.
local function blackAndWhite(frame)
  for i = 0, #frame - 1, 4 do
    local luma = (frame[i] * 77 + frame[i + 1] * 150 + frame[i + 2] * 29) >> 8
    frame[i], frame[i + 1], frame[i + 2] = luma, luma, luma
  end
end

async.spawn(function()
  local frames, spent, since = 0, 0, datetime.now():millis()

  while true do
    local camera = host.cameraFrame()
    if camera.error then
      print(camera.error)
      return
    end

    if camera.frame then
      local started = datetime.now():millis()
      blackAndWhite(camera.frame)
      host.cameraShow({ frame = camera.frame })
      frames = frames + 1
      spent = spent + datetime.now():millis() - started
    end

    -- Once a second the console tells how fast Lua keeps up with the camera.
    if datetime.now():millis() - since >= 1000 then
      print(string.format("%d frames a second, %.1f ms of Lua for each frame.", frames, spent / math.max(frames, 1)))
      frames, spent, since = 0, 0, datetime.now():millis()
    end

    async.sleep(5):await()
  end
end)

print(string.format("Allow the camera, and press Stop to end the demo. Each frame is %d bytes.", width * height * 4))
`,
  },
  {
    module: "dataview",
    label: "dataview — a picture made of bytes",
    code: `local dataview = require("dataview")

-- A data view is plain memory, here one byte for each point of a small picture, which Lua fills and reads in place.
local width, height = 56, 22
local picture = dataview.new(width * height)
print(tostring(picture) .. ", with the id " .. picture.id .. ".")

-- Each byte counts how many steps a point of the plane takes to escape the Mandelbrot set, up to 255.
for y = 0, height - 1 do
  for x = 0, width - 1 do
    local cr, ci = -2.2 + 3 * (x + 0.5) / width, -1.2 + 2.4 * (y + 0.5) / height
    local zr, zi, steps = 0.0, 0.0, 0
    while steps < 255 and zr * zr + zi * zi < 4 do
      zr, zi = zr * zr - zi * zi + cr, 2 * zr * zi + ci
      steps = steps + 1
    end
    picture[y * width + x] = steps
  end
end

-- The same bytes print as shades, read one at a time without copying the view into a string.
local shades = " .:-=+*#%@"
for y = 0, height - 1 do
  local row = {}
  for x = 0, width - 1 do
    local steps = picture[y * width + x]
    local shade = steps == 255 and #shades or 1 + math.floor(math.log(steps) / math.log(255) * (#shades - 1))
    row[#row + 1] = shades:sub(shade, shade)
  end
  print(table.concat(row))
end

-- A range comes out as a string when one is wanted, and a string goes back in with one write.
local hex = picture:read(width * (height // 2), 8):gsub(".", function(byte)
  return string.format("%02x ", byte:byte())
end)
print("Eight bytes of the middle row: " .. hex)
picture:write(0, "Varn")
print("The first bytes after a write:", picture[0], picture[1], picture[2], picture[3])

-- Releasing frees the memory now, and the value refuses any later use.
picture:release()
local _, refused = pcall(function()
  return picture[0]
end)
print("Released: " .. tostring(picture.released) .. ". " .. refused:gsub("^[^:]+:%d+: ", ""))
`,
  },
  {
    module: "host",
    label: "host — ask the page",
    code: `local datetime = require("datetime")

-- The page offers functions to Lua under the global table \`host\`, the same bridge a mobile or desktop app offers, and every value crosses it as JSON.
local page = host.pageInfo()
print("Language: " .. page.language)
print("Preferred languages: " .. table.concat(page.languages, ", "))
print("Online: " .. tostring(page.online))
print("Logical cores: " .. page.cores)
print("Time zone: " .. page.timeZone)

-- The page and Lua read the clock of the same machine, so their answers agree.
local pageNow = datetime.fromMillis(page.now)
local luaNow = datetime.now()
print("Page clock: " .. pageNow:iso())
print("Lua clock: " .. luaNow:iso())
print(string.format("Apart by %d ms.", math.abs(luaNow:diffIn(pageNow, "millis"))))
`,
  },
  {
    module: "http",
    label: "http — url encode & decode",
    code: `local http = require("http")

-- Percent-encoding works in every build, including the browser.
local q = http.urlEncode("a b & c=d/e")
print("encoded:", q)
print("decoded:", http.urlDecode(q))
print("form plus:", http.urlDecode("name=jo%C3%A3o+silva"))
`,
  },
  {
    module: "http",
    label: "http — client fetch",
    code: `local async = require("async")
local http = require("http")

-- The HTTP client works in the browser through the Fetch API, and the route \`/delay/N\` of httpbin answers after N seconds so the await is visible.
async.spawn(function()
  print("Requesting with a 2s delay...")
  local res, err = http.client.get("https://httpbin.org/delay/2"):await()
  if err then
    print("Request failed:", err)
    return
  end

  print("status:", res.status)
  print("ok:", res.ok)
end)

print("The main chunk returned before the response arrived.")
`,
  },
  {
    module: "http",
    label: "http — stream a body as it arrives",
    code: `local async = require("async")
local datetime = require("datetime")
local http = require("http")

-- The server drips ten bytes over three seconds, and each piece reaches Lua the moment it arrives rather than once the body ends.
async.spawn(function()
  local started = datetime.now():millis()
  local received = 0

  local function stamp(text)
    print(string.format("%5d ms  %s", datetime.now():millis() - started, text))
  end

  local _, err = http.client.stream({
    url = "https://httpbin.org/drip?duration=3&numbytes=10&delay=0",
    onResponse = function(status, headers)
      local length = headers["content-length"]
      stamp(string.format("Status %d, with %s.", status, length and length .. " bytes announced" or "no length announced"))
    end,
  }, function(chunk)
    received = received + #chunk
    stamp(string.format("Received %d %s, %d in all.", #chunk, #chunk == 1 and "byte" or "bytes", received))
  end):await()

  if err then
    stamp("The stream failed: " .. err)
    return
  end

  stamp("The body ended.")
end)
`,
  },
  {
    module: "http",
    label: "http — query, JSON body & status",
    code: `local async = require("async")
local crypto = require("crypto")
local http = require("http")

async.spawn(function()
  -- A query table becomes a sorted, percent-encoded query string.
  local got, err = http.client.get("https://httpbin.org/get", { query = { city = "São Paulo", page = 2 } }):await()
  if err then
    print("The request failed: " .. err)
    return
  end
  print("GET " .. got.status .. ", the server read the address " .. got.json().url)

  -- The option \`json\` encodes the body and sets its content type, and the headers travel as given.
  local posted = http.client.post("https://httpbin.org/post", {
    json = { order = 1042, items = { "tea", "bread", "jam" }, gift = false },
    headers = { ["X-Session"] = crypto.uuidV4() },
  }):await()
  local echo = posted.json()
  print(string.format("POST %d, the server read order %d with %d items.", posted.status, echo.json.order, #echo.json.items))
  print("Content type sent: " .. echo.headers["Content-Type"])
  print("Session sent: " .. echo.headers["X-Session"])

  -- A status that reports a failure is still a response, with \`ok\` set to false.
  local missing = http.client.get("https://httpbin.org/status/404"):await()
  print(string.format("A missing page answers %d, and ok is %s.", missing.status, tostring(missing.ok)))
end)
`,
  },
  {
    module: "http",
    label: "http — download, read in chunks & cancel",
    code: `local async = require("async")
local datetime = require("datetime")
local fs = require("fs")
local http = require("http")

async.spawn(function()
  local started = datetime.now():millis()

  local function stamp(text)
    print(string.format("%5d ms  %s", datetime.now():millis() - started, text))
  end

  -- The body goes into a partial file as it arrives and becomes the file only once it is whole, so memory holds one piece at a time.
  local nextReport = 0
  local saved, err = http.client.download({
    url = "https://httpbin.org/drip?duration=2&numbytes=4000&delay=0",
    path = "downloads/drip.bin",
    onProgress = function(received, total)
      if received >= nextReport then
        stamp(string.format("Downloaded %d of %s bytes.", received, total or "an unknown number of"))
        nextReport = received + 800
      end
    end,
  }):await()
  if not saved then
    stamp("The download failed: " .. err)
    return
  end
  stamp(string.format("Saved %d bytes to %s, with no partial file left: %s.", saved.size, saved.path, tostring(not fs.exists(saved.path .. ".part"))))

  -- A response from \`open\` hands its body over one chunk per read, and the chunks not read yet hold the transfer back.
  local response <close> = http.client.open({ url = "https://httpbin.org/stream/4" }):await()
  stamp("Opened with the status " .. response.status .. ".")
  local lines = 0
  while true do
    local chunk = response:read():await()
    if chunk == nil then
      break
    end
    lines = lines + select(2, chunk:gsub("\\n", ""))
    stamp(string.format("Read a chunk of %d bytes.", #chunk))
  end
  stamp("The body held " .. lines .. " lines of JSON.")

  -- A controller cancels every request that names it, and a cancelled download leaves no file behind.
  local controller = http.client.controller()
  async.spawn(function()
    async.sleep(500):await()
    controller:cancel()
  end)
  local _, cancelled = http.client.download({ url = "https://httpbin.org/delay/5", path = "downloads/slow.json", controller = controller }):await()
  stamp("The slow download ended: " .. cancelled)
  stamp("Nothing was left behind: " .. tostring(not fs.exists("downloads/slow.json") and not fs.exists("downloads/slow.json.part")) .. ".")
end)
`,
  },
  {
    module: "fs",
    label: "fs — read & write (MEMFS)",
    code: `local async = require("async")
local fs = require("fs")

async.spawn(function()
  fs.writeFile("demo.txt", "hello from lua\\n"):await()

  local content = fs.readFile("demo.txt"):await()
  print("read back:", content)
end)
`,
  },
  {
    module: "fs",
    label: "fs — folders, stat & streaming",
    code: `local async = require("async")
local fs = require("fs")

async.spawn(function()
  -- The page starts with an empty file system in memory, so the example builds its own tree in a fresh folder.
  local root = fs.mkdtemp("journal-"):await()
  fs.mkdir(root .. "/2026/11"):await()

  -- Appending adds entries without reading the file first.
  local notes = root .. "/2026/11/notes.txt"
  for day = 1, 5 do
    fs.append(notes, string.format("Day %d, %d lines of Lua.\\n", day, day * 12)):await()
  end
  local info = fs.stat(notes):await()
  print(string.format("The notes hold %d bytes, and isFile is %s.", info.size, tostring(info.isFile)))

  -- A copy and a rename keep an archive next to them.
  fs.copy(notes, root .. "/2026/11/backup.txt"):await()
  fs.rename(root .. "/2026/11/backup.txt", root .. "/2026/archive.txt"):await()

  -- The tree is walked with \`readdir\` and \`stat\`.
  local function walk(path, depth)
    local names = fs.readdir(path):await()
    table.sort(names)
    for _, name in ipairs(names) do
      local child = path .. "/" .. name
      local stat = fs.stat(child):await()
      print(string.rep("  ", depth) .. name .. (stat.isDir and "/" or string.format(", %d bytes", stat.size)))
      if stat.isDir then
        walk(child, depth + 1)
      end
    end
  end
  print(root .. "/")
  walk(root, 1)

  -- A handle streams the file in small reads, which keeps a large file out of memory.
  local handle = fs.open(notes, "r"):await()
  local pending, lines = "", 0
  while true do
    local chunk = handle:read(16):await()
    if chunk == "" then
      break
    end
    pending = pending .. chunk
    for line in pending:gmatch("([^\\n]*)\\n") do
      lines = lines + 1
      print(string.format("Line %d: %s", lines, line))
    end
    pending = pending:match("[^\\n]*$")
  end
  handle:close():await()

  fs.removeRecursive(root):await()
  print("The folder is gone: " .. tostring(not fs.exists(root)))
end)
`,
  },
  {
    module: "zip",
    label: "zip — create & list",
    code: `local async = require("async")
local zip = require("zip")

async.spawn(function()
  -- A source file is written into MEMFS.
  local file = io.open("hello.txt", "w")
  file:write("zipped!\\n")
  file:close()

  local _, err = zip.create("demo.zip", { { file = "hello.txt", entry = "a/hello.txt" } }):await()
  if err then
    print("zip error:", err)
    return
  end

  local names = {}
  for _, entry in ipairs(zip.list("demo.zip"):await()) do
    names[#names + 1] = entry.name .. " (" .. entry.size .. " bytes)"
  end
  print("entries:", table.concat(names, ", "))
end)
`,
  },
  {
    module: "zip",
    label: "zip — round trip & unsafe entries",
    code: `local async = require("async")
local crypto = require("crypto")
local fs = require("fs")
local zip = require("zip")

async.spawn(function()
  -- A small site, with one page repetitive enough to compress well and one file of random bytes that does not.
  local root = fs.mkdtemp("site-"):await()
  fs.mkdir(root .. "/site/css"):await()
  local files = {
    { entry = "index.html", content = "<h1>Hello</h1>\\n" .. string.rep("<p>Lua everywhere.</p>\\n", 60) },
    { entry = "css/main.css", content = "body { margin: 0; font-family: sans-serif; }\\n" },
    { entry = "seed.bin", content = crypto.randomBytes(512) },
  }
  local entries = {}
  for i, file in ipairs(files) do
    file.path = root .. "/site/" .. file.entry
    fs.writeFile(file.path, file.content):await()
    entries[i] = { file = file.path, entry = file.entry }
  end

  local archive = root .. "/site.zip"
  zip.create(archive, entries):await()
  print(string.format("The archive takes %d bytes.", fs.stat(archive):await().size))
  for _, entry in ipairs(zip.list(archive):await()) do
    print(string.format("  %-12s %5d bytes, stored in %4d, %3d%%", entry.name, entry.size, entry.compressedSize, math.floor(100 * entry.compressedSize / entry.size)))
  end

  -- Extracting into a new folder and comparing digests shows that every byte survived.
  zip.extract(archive, root .. "/copy"):await()
  for _, file in ipairs(files) do
    local restored = fs.readFile(root .. "/copy/" .. file.entry):await()
    local same = crypto.digest("SHA256", restored) == crypto.digest("SHA256", file.content)
    print(string.format("%s comes back %s.", file.entry, same and "identical" or "changed"))
  end

  -- An entry that would climb out of the folder it is extracted into is refused before the archive is written.
  local _, err = zip.create(root .. "/evil.zip", { { file = files[2].path, entry = "../escaped.css" } }):await()
  print("Refused: " .. err)
  print("The archive exists: " .. tostring(fs.exists(root .. "/evil.zip")))

  fs.removeRecursive(root):await()
end)
`,
  },
  {
    module: "json",
    label: "json — encode & decode",
    code: `local json = require("json")

local text = json.encode({ name = "varn", tags = { "fast", "small" }, version = 1 })
print("encoded:", text)

local value = json.decode(text)
print("name:", value.name)
print("first tag:", value.tags[1])

print("pretty:")
print(json.encode({ user = { id = 1, roles = { "admin" } } }, { pretty = true }))
`,
  },
  {
    module: "json",
    label: "json — null, markers & errors",
    code: `local json = require("json")

-- A nil inside a table leaves the key out, while \`json.null\` keeps it as null.
print(json.encode({ name = "Ada", nickname = json.null }))

-- Decoding with \`nullValue\` keeps every null, so keys and the length of arrays survive.
local record = json.decode('{"tags":["a",null,"c"],"manager":null}', { nullValue = json.null })
print(string.format("Tags: %d, and the manager is null: %s.", #record.tags, tostring(record.manager == json.null)))

-- An empty table says which one it is through a marker.
print(json.encode({ list = json.array(), map = json.object() }))

-- Indented output sorts the keys, so the same value always prints the same way.
print(json.encode({ zeta = 1, alpha = { 3, 2, 1 }, middle = { ok = true } }, { indent = 4 }))

-- A broken document raises with its line and column.
local _, decodeError = pcall(json.decode, '{\\n  "price": 12,\\n  "tax": ,\\n}')
print("Decoding failed: " .. decodeError)

-- A value JSON cannot carry is named by its key.
local _, encodeError = pcall(json.encode, { onClick = print })
print("Encoding failed: " .. encodeError)
`,
  },
  {
    module: "xml",
    label: "xml — encode & decode",
    code: `local xml = require("xml")

-- A document is built from the node model.
local doc = xml.encode({
  name = "note",
  attributes = { priority = "high" },
  children = {
    { name = "to", text = "Lua" },
    { name = "from", text = "C++" },
  },
}, { pretty = true })
print(doc)

-- It parses back into the same node model.
local node = xml.decode(doc)
print("root:", node.name)
print("priority:", node.attributes.priority)
print("first child:", node.children[1].name, node.children[1].text)
`,
  },
  {
    module: "xml",
    label: "xml — read a feed",
    code: `local datetime = require("datetime")
local xml = require("xml")

local feed = [[
<?xml version="1.0" encoding="UTF-8"?>
<feed title="Release notes">
  <entry id="3" published="2026-10-28T09:00:00Z"><title>Faster pools</title><tag>runtime</tag><tag>speed</tag></entry>
  <entry id="2" published="2026-09-14T16:30:00Z"><title>Shared memory</title><tag>dataview</tag></entry>
  <entry id="1" published="2026-08-01T12:00:00Z"><title>Hello &amp; welcome</title><tag>news</tag></entry>
</feed>]]

-- The document becomes a tree of nodes with a name, attributes, children and text.
local root = xml.decode(feed)
print(root.attributes.title .. ", " .. #root.children .. " entries")

for _, entry in ipairs(root.children) do
  local title, tags = "", {}
  for _, child in ipairs(entry.children) do
    if child.name == "title" then
      title = child.text
    elseif child.name == "tag" then
      tags[#tags + 1] = child.text
    end
  end

  local published = datetime.parse(entry.attributes.published)
  print(string.format("  #%s  %s  %-16s %s", entry.attributes.id, published:format("%d %b %Y"), title, table.concat(tags, ", ")))
end

-- Building a document escapes text and attributes on its own.
print(xml.encode({
  name = "entry",
  attributes = { title = 'Fish & "chips"' },
  children = { { name = "body", text = "1 < 2 > 0" } },
}, { pretty = true }))
`,
  },
  {
    module: "crypto",
    label: "crypto — digests, hmac, codecs, uuid",
    code: `local crypto = require("crypto")

print("sha256:", crypto.digest("SHA256", "varn"))
print("sha512:", crypto.digest("SHA512", "varn"):sub(1, 32) .. "…")
print("hmac:", crypto.hmac("SHA256", "secret-key", "message"))

print("base64:", crypto.base64Encode("hello, varn"))
print("hex:", crypto.hexEncode("abc"))

print("uuid v4:", crypto.uuidV4())
print("uuid v7:", crypto.uuidV7())
print("random:", crypto.hexEncode(crypto.randomBytes(8)))

-- AES-GCM, scrypt and PBKDF2 are native-only and raise in the browser build.
`,
  },
  {
    module: "crypto",
    label: "crypto — sign & verify a token",
    code: `local crypto = require("crypto")
local json = require("json")

local secret = crypto.randomBytes(32)

-- A token is a header, a payload and a signature, each in URL-safe base64, and the signature is an HMAC of the other two.
local function signature(head, body)
  return crypto.base64UrlEncode(crypto.hmac("SHA256", secret, head .. "." .. body, "raw"))
end

local function sign(claims)
  local head = crypto.base64UrlEncode(json.encode({ alg = "HS256", typ = "JWT" }))
  local body = crypto.base64UrlEncode(json.encode(claims))
  return head .. "." .. body .. "." .. signature(head, body)
end

-- Verifying computes the signature again and compares in constant time, so the time it takes reveals nothing.
local function verify(token)
  local head, body, signed = token:match("^([^.]+)%.([^.]+)%.([^.]+)$")
  if not head then
    return nil, "The token is malformed."
  end
  if not crypto.equals(signature(head, body), signed) then
    return nil, "The signature does not match."
  end
  return json.decode(crypto.base64UrlDecode(body))
end

local token = sign({ sub = crypto.uuidV7(), role = "editor", exp = os.time() + 3600 })
print("Token: " .. token)

local claims = verify(token)
print(string.format("Verified the %s %s until %s.", claims.role, claims.sub, os.date("!%H:%M UTC", claims.exp)))

-- Changing one claim and keeping the old signature is caught.
local head, _, signed = token:match("^([^.]+)%.([^.]+)%.([^.]+)$")
local forged = head .. "." .. crypto.base64UrlEncode(json.encode({ sub = claims.sub, role = "admin", exp = claims.exp })) .. "." .. signed
print("Forged token: " .. select(2, verify(forged)))

-- The browser build carries digests, HMAC, codecs, identifiers and random bytes, and the ciphers raise a clear error here.
local encrypted, sealed = pcall(crypto.encrypt, crypto.randomBytes(32), "secret")
print(encrypted and string.format("Encryption sealed 6 bytes into %d.", #sealed) or "Encryption: " .. sealed)
`,
  },
  {
    module: "datetime",
    label: "datetime — parse, format, math",
    code: `local datetime = require("datetime")

local d = datetime.parse("2026-06-21T12:30:00Z")
print("iso:", d:iso())
print("weekday:", d:weekdayName(), "day of year:", d:fields().yearday)

-- Calendar-aware arithmetic clamps to the end of a short month.
print("jan 31 + 1 month:", datetime.parse("2026-01-31"):add({ months = 1 }):iso())

-- A difference is measured in plain or calendar units.
print("days apart:", datetime.parse("2026-03-15"):diffIn(datetime.parse("2026-01-10"), "days"))

-- The same instant renders at a fixed UTC offset.
print("in +05:30:", d:iso(330))
`,
  },
  {
    module: "datetime",
    label: "datetime — month calendar",
    code: `local datetime = require("datetime")

-- A calendar of the current month, with today marked, built from the fields of an instant.
local today = datetime.now()
local first = today:startOf("month")
local title = first:monthName() .. " " .. first:year()
print(string.rep(" ", (27 - #title) // 2) .. title)
print(" Mon Tue Wed Thu Fri Sat Sun")

local cells = { string.rep("    ", first:weekday() - 1) }
for day = 1, today:daysInMonth() do
  cells[#cells + 1] = string.format("%4s", day == today:day() and "[" .. day .. "]" or day)
  if (first:weekday() - 1 + day) % 7 == 0 then
    print(table.concat(cells))
    cells = {}
  end
end
if #cells > 0 then
  print(table.concat(cells))
end

-- Calendar arithmetic counts what people mean by a month or a year.
local newYear = today:startOf("year"):add({ years = 1 })
print()
print(string.format("%d days until %s.", newYear:diffIn(today, "days"), newYear:format("%A, %d %B %Y")))
print("The month ends at " .. today:endOf("month"):iso() .. ".")
print("Three months from today is " .. today:add({ months = 3 }):format("%Y-%m-%d") .. ".")
print("This is a leap year: " .. tostring(today:isLeapYear()) .. ".")

-- One instant reads differently at each fixed offset.
for _, zone in ipairs({ { "UTC", 0 }, { "UTC-3", -180 }, { "UTC+5:30", 330 }, { "UTC+9", 540 } }) do
  print(string.format("%-9s %s", zone[1], today:format("%H:%M on %A", zone[2])))
end
`,
  },
  {
    module: "log",
    label: "log — levels",
    code: `local log = require("log")

log.debug("debug", 1)
log.info("info", "hello")
log.warn("careful")
log.error("boom", { code = 7 })

print("Log lines were emitted.")
`,
  },
  {
    module: "log",
    label: "log — fields & a file",
    code: `local async = require("async")
local fs = require("fs")
local log = require("log")

async.spawn(function()
  -- The lines also go to a file, which here lives in the file system the page keeps in memory.
  local folder = fs.mkdtemp("logs-"):await()
  log.toFile(folder .. "/app.log")
  log.setLevel("info")

  log.debug("This line is below the level, so it is dropped.")
  log.info("Server ready", { port = 8080, workers = 4 })
  log.warn("Slow request", { path = "/search", ms = 870 })
  log.error("Payment failed", { order = 1042, reason = "declined" })

  log.setLevel("debug")
  log.debug("Visible again once the level drops.")

  -- The browser console shows every line as it is written, and the file holds the same lines.
  print("The file holds:")
  print(fs.readFile(folder .. "/app.log"):await())
end)
`,
  },
  {
    module: "platform",
    label: "platform — system info",
    code: `local p = require("platform")

print("os", p.os())
print("arch", p.arch())
print("cpus", p.cpuCount())
print("pointer bytes", p.pointerSize())
print("endianness", p.endianness())
print("host version", p.hostVersion())
`,
  },
  {
    module: "platform",
    label: "platform — what this build can do",
    code: `local async = require("async")
local platform = require("platform")

local version = platform.version
print(string.format("Varn %s on %s, %s, with %d-bit pointers.", version.string, platform.os(), platform.arch(), platform.pointerSize() * 8))
print(string.format("The version also comes as numbers, major %d, minor %d and patch %d, so a component checks it without parsing the text.", version.major, version.minor, version.patch))
print()

-- Every module loads in every build, and one this build lacks raises a clear error when it is called, so a script learns what it can use.
local probes = {
  { "json", function() return require("json").encode({ 1 }) end },
  { "crypto, digests", function() return require("crypto").digest("SHA256", "") end },
  { "crypto, ciphers", function() return require("crypto").encrypt(string.rep("k", 32), "text") end },
  { "fs", function() return require("fs").exists("/") end },
  { "http, server", function() return require("http").createServer(function() end) end },
  { "socket", function() return require("socket").resolve("localhost"):await() end },
  { "process", function() return require("process").exec("echo hello"):await() end },
  { "ffi", function() return require("ffi").cdef("int abs(int);") end },
}

async.spawn(function()
  for _, probe in ipairs(probes) do
    local ok, result, problem = pcall(probe[2])
    local reason = not ok and result or problem
    if reason then
      print(string.format("%-16s Missing. %s", probe[1], (tostring(reason):gsub("^[^:]+:%d+: ", ""))))
    else
      print(string.format("%-16s Available.", probe[1]))
    end
  end
end)
`,
  },
];
