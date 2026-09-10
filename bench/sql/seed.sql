-- benchmark fixture: a small indexed table to read random rows from
CREATE TABLE IF NOT EXISTS world (
    id INT PRIMARY KEY,
    randomNumber INT NOT NULL
);

SET SESSION cte_max_recursion_depth = 20000;

INSERT INTO world (id, randomNumber)
WITH RECURSIVE seq(n) AS (
    SELECT 1
    UNION ALL
    SELECT n + 1 FROM seq WHERE n < 10000
)
SELECT n, FLOOR(1 + RAND() * 10000) FROM seq;
