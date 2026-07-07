-- This example demonstrates working with tables from multiple clusters in a single query.
-- All you need to do is specify the cluster name before the dot and the table: cluster.`//path/to/table`
-- Below tables from different clusters are joined.
SELECT
    Max_by(p.price, p.date) as last_price
   ,n.name
FROM tundra.`$price` p
JOIN (
        SELECT
            id, name
        FROM dirac.`$nomenclature`
    ) n on n.id = p.nomenclature_id
WHERE StartsWith(n.name, "Bi")
GROUP BY n.name
ORDER BY n.name;

-- If you use a special function to read from multiple tables as if they were one
-- (PARTITIONS, RANGE, CONCAT, etc), the cluster must be specified before that function.
SELECT
    count(*) as orders_count,
    table_date
FROM (
    SELECT * FROM dirac.PARTITIONS('$orders', '${table_date:Date}', 'plain')
    UNION ALL
    SELECT * FROM tundra.PARTITIONS('$orders', '${table_date:Date}', 'plain')
)
GROUP BY table_date
ORDER BY table_date;
