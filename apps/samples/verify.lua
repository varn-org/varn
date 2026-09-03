-- Runs every sample the way the mobile apps do, so a sample that only works from a shell never ships.
-- Each one gets its own process, because that is what gives it a fresh runtime whose event loop drains.
-- Before the output is read, exactly like one tap of Run.
local async = require("async")
local fs = require("fs")
local json = require("json")
local process = require("process")

local dir = arg[0]:match("^(.*)[/\\]") or "."
local scratch = assert(os.getenv("VARN_TEST_DIR"), "VARN_TEST_DIR is not set")

-- The host binary sits at the lowest argument index, below the script and any host option.
local function hostBinary()
    local index = 0
    while arg[index - 1] ~= nil do
        index = index - 1
    end

    return arg[index]
end

-- The same harness the apps install: print captured to a file, a writable directory named, and an.
-- Error inside a background coroutine reported rather than lost.
local function harness(sample, output, sampleScratch)
    return string.format(
        [[
local out = assert(io.open([==[%s]==], "w"))

local function report(message)
    out:write("error: ", tostring(message), "\n")
    out:flush()
end

print = function(...)
    local parts = {}
    for i = 1, select("#", ...) do
        parts[i] = tostring((select(i, ...)))
    end
    out:write(table.concat(parts, "\t"), "\n")
    out:flush()
end

SAMPLE_DIR = [==[%s]==]

local async = require("async")
local realRun, realSpawn = async.run, async.spawn

async.run = function(fn)
    return realRun(function()
        local ok, err = pcall(fn)
        if not ok then
            report(err)
        end
    end)
end

async.spawn = function(fn)
    return realSpawn(function()
        local ok, err = pcall(fn)
        if not ok then
            report(err)
        end
    end)
end

local file = assert(io.open([==[%s]==], "r"))
local source = file:read("a")
file:close()

local chunk, loadError = load(source, "=sample")
if not chunk then
    report(loadError)
else
    local ok, runError = pcall(chunk)
    if not ok then
        report(runError)
    end
end
]],
        output,
        sampleScratch,
        sample
    )
end

async.run(function()
    local file = assert(io.open(dir .. "/manifest.json", "r"))
    local samples = json.decode(file:read("a")).samples
    file:close()

    local binary = hostBinary()
    local failures = 0

    for _, entry in ipairs(samples) do
        local sampleScratch = scratch .. "/scratch"
        local harnessPath = scratch .. "/harness.lua"
        local outputPath = scratch .. "/output.txt"

        fs.removeRecursive(sampleScratch):await()
        fs.mkdir(sampleScratch):await()
        fs.writeFile(harnessPath, harness(dir .. "/lua/" .. entry.file, outputPath, sampleScratch)):await()
        fs.writeFile(outputPath, ""):await()

        local result = process.exec(string.format('%q %q', binary, harnessPath)):await()
        local captured = fs.readFile(outputPath):await() or ""

        local failure
        if captured:find("^error: ") or captured:find("\nerror: ") then
            failure = captured:match("error: ([^\n]*)")
        elseif result.code ~= 0 then
            failure = "engine exited with code " .. tostring(result.code)
        elseif captured == "" then
            failure = "printed nothing"
        end

        if failure then
            failures = failures + 1
            print(string.format("FAIL  %-24s %s", entry.file, failure))
        else
            local lines = {}
            for line in captured:gmatch("[^\n]+") do
                lines[#lines + 1] = line
            end
            print(string.format("ok    %-24s %s", entry.file, (lines[#lines] or ""):sub(1, 46)))
        end
    end

    print(string.format("\n%d samples, %d failed", #samples, failures))

    if failures > 0 then
        os.exit(1)
    end
end)
