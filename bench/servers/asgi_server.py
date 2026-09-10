# python server for the comparison benchmark as a raw ASGI app with no framework.
# /plaintext and /json are framework-free baselines.
# /db reads a random row through an aiomysql pool and /cache hits redis through redis.asyncio, both async.
# run with uvicorn on uvloop, scaling with --workers N.
import json
import os
import random

import aiomysql
import redis.asyncio as aioredis

_pool = None
_redis = None


async def _get_pool():
    global _pool
    if _pool is None:
        _pool = await aiomysql.create_pool(
            host=os.getenv("MYSQL_HOST", "127.0.0.1"),
            port=int(os.getenv("MYSQL_PORT", "3306")),
            user=os.getenv("MYSQL_USER", "bench"),
            password=os.getenv("MYSQL_PASSWORD", "benchpass"),
            db=os.getenv("MYSQL_DB", "bench"),
            minsize=1,
            maxsize=int(os.getenv("POOL_SIZE", "16")),
            autocommit=True,
        )
    return _pool


def _get_redis():
    global _redis
    if _redis is None:
        _redis = aioredis.Redis(
            host=os.getenv("REDIS_HOST", "127.0.0.1"),
            port=int(os.getenv("REDIS_PORT", "6379")),
        )
    return _redis


async def _respond(send, status, content_type, body):
    await send(
        {
            "type": "http.response.start",
            "status": status,
            "headers": [(b"content-type", content_type)],
        }
    )
    await send({"type": "http.response.body", "body": body})


async def app(scope, receive, send):
    assert scope["type"] == "http"
    path = scope["path"]

    if path == "/plaintext":
        await _respond(send, 200, b"text/plain", b"Hello, World!")
        return

    if path == "/json":
        await _respond(send, 200, b"application/json", json.dumps({"hello": "world"}).encode())
        return

    if path == "/db":
        row_id = random.randint(1, 10000)
        pool = await _get_pool()
        async with pool.acquire() as conn:
            async with conn.cursor() as cur:
                await cur.execute("SELECT randomNumber FROM world WHERE id = %s", (row_id,))
                row = await cur.fetchone()
        body = json.dumps({"id": row_id, "randomNumber": row[0]}).encode()
        await _respond(send, 200, b"application/json", body)
        return

    if path == "/cache":
        count = await _get_redis().incr("bench:hits")
        await _respond(send, 200, b"application/json", json.dumps({"count": count}).encode())
        return

    await _respond(send, 404, b"text/plain", b"not found")
