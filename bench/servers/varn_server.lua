-- varn server for the comparison benchmark.
-- /plaintext and /json are framework-free baselines.
-- /db reads a random row through vdo over a connection pool (each blocking-free query needs its own connection).
-- /cache hits redis over a single multiplexed connection that auto-pipelines concurrent commands, the ioredis model.
-- scale with VARN_WORKERS=N.
package.path = ".bench-components/?.lua;.bench-components/?/init.lua;" .. package.path

local http = require("http")
local async = require("async")
local vdo = require("vdo")
local redis = require("redis")
local pool = require("pool")

local port = tonumber(os.getenv("PORT") or "3000")
local poolSize = tonumber(os.getenv("POOL_SIZE") or "16")

local dbPool = pool.new({
    size = poolSize,
    connect = function()
        local dsn = string.format(
            "mysql:host=%s;port=%s;dbname=%s",
            os.getenv("MYSQL_HOST") or "127.0.0.1",
            os.getenv("MYSQL_PORT") or "3306",
            os.getenv("MYSQL_DB") or "bench"
        )
        return vdo.connect(dsn, os.getenv("MYSQL_USER") or "bench", os.getenv("MYSQL_PASSWORD") or "benchpass")
    end,
})

-- one multiplexed redis connection shared by every request, connected lazily on the first /cache hit.
-- redisGate serializes the concurrent first hits so exactly one connection is opened.
local redisClient
local redisGate

local function redisConn()
    if redisClient then
        return redisClient
    end
    if redisGate then
        redisGate:await()
        return redisClient
    end

    local gate, done = async.deferred()
    redisGate = gate

    local ok, client = pcall(redis.connect, {
        host = os.getenv("REDIS_HOST") or "127.0.0.1",
        port = tonumber(os.getenv("REDIS_PORT") or "6379"),
        pipeline = true,
    })
    if ok then
        redisClient = client
    end

    redisGate = nil
    done()

    if not ok then
        error(client)
    end
    return redisClient
end

http.createServer(function(req, res)
    local path = req.path

    if path == "/plaintext" then
        res:setHeader("Content-Type", "text/plain")
        res:finish("Hello, World!")
        return
    end

    if path == "/json" then
        res:json({ hello = "world" })
        return
    end

    if path == "/db" then
        local id = math.random(1, 10000)
        local rows = dbPool:with(function(db)
            return db:query("SELECT randomNumber FROM world WHERE id = ?", { id }):fetchAll()
        end)
        res:json({ id = id, randomNumber = rows[1].randomNumber })
        return
    end

    if path == "/cache" then
        local hits = redisConn():incr("bench:hits")
        res:json({ count = hits })
        return
    end

    res:status(404)
    res:finish("not found")
end):listen({ host = "127.0.0.1", port = port, servePublic = false })
