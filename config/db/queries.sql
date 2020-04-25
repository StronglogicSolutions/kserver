SELECT
   s.id, s.mask, s.time,
   a.name AS application,
   (SELECT string_agg(name, ',') AS files FROM file WHERE sid = s.id)
FROM schedule s
INNER JOIN apps a
ON a.mask = s.mask;
