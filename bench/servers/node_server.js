// node server for the comparison benchmark using raw http with no framework
// /plaintext and /json are baselines while /db reads a random row through a mysql2 pool and /cache hits redis through ioredis
// scale across cores with WORKERS=N through the built-in cluster module
const http = require("http");
const cluster = require("cluster");
const mysql = require("mysql2/promise");
const Redis = require("ioredis");

const port = parseInt(process.env.PORT || "3000", 10);
const workers = parseInt(process.env.WORKERS || "1", 10);
const poolSize = parseInt(process.env.POOL_SIZE || "16", 10);

function serve() {
  const db = mysql.createPool({
    host: process.env.MYSQL_HOST || "127.0.0.1",
    port: parseInt(process.env.MYSQL_PORT || "3306", 10),
    user: process.env.MYSQL_USER || "bench",
    password: process.env.MYSQL_PASSWORD || "benchpass",
    database: process.env.MYSQL_DB || "bench",
    connectionLimit: poolSize,
  });

  const redis = new Redis({
    host: process.env.REDIS_HOST || "127.0.0.1",
    port: parseInt(process.env.REDIS_PORT || "6379", 10),
  });

  http
    .createServer(async (req, res) => {
      try {
        if (req.url === "/plaintext") {
          res.writeHead(200, { "Content-Type": "text/plain" });
          res.end("Hello, World!");
        } else if (req.url === "/json") {
          res.writeHead(200, { "Content-Type": "application/json" });
          res.end(JSON.stringify({ hello: "world" }));
        } else if (req.url === "/db") {
          const id = 1 + Math.floor(Math.random() * 10000);
          const [rows] = await db.query("SELECT randomNumber FROM world WHERE id = ?", [id]);
          res.writeHead(200, { "Content-Type": "application/json" });
          res.end(JSON.stringify({ id, randomNumber: rows[0].randomNumber }));
        } else if (req.url === "/cache") {
          const count = await redis.incr("bench:hits");
          res.writeHead(200, { "Content-Type": "application/json" });
          res.end(JSON.stringify({ count }));
        } else {
          res.writeHead(404);
          res.end("not found");
        }
      } catch (e) {
        res.writeHead(500);
        res.end(String(e && e.message ? e.message : e));
      }
    })
    .listen(port, "127.0.0.1");
}

if (workers > 1 && cluster.isPrimary) {
  for (let i = 0; i < workers; i++) cluster.fork();
} else {
  serve();
}
