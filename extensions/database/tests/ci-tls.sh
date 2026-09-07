#!/usr/bin/env bash
set -euo pipefail
mkdir -p build/database-tls
openssl req -x509 -newkey rsa:2048 -nodes -keyout build/database-tls/ca-key.pem -out build/database-tls/ca.pem -days 2 -subj '/CN=HHY CI Test CA' 2>/dev/null
openssl req -newkey rsa:2048 -nodes -keyout build/database-tls/server-key.pem -out build/database-tls/server.csr -subj '/CN=127.0.0.1' 2>/dev/null
printf 'subjectAltName=IP:127.0.0.1\nextendedKeyUsage=serverAuth\n' > build/database-tls/extensions.cnf
openssl x509 -req -in build/database-tls/server.csr -CA build/database-tls/ca.pem -CAkey build/database-tls/ca-key.pem -CAcreateserial -out build/database-tls/server.pem -days 2 -extfile build/database-tls/extensions.cnf 2>/dev/null
for container in "$MYSQL_CONTAINER" "$PG_CONTAINER"; do
  docker cp build/database-tls/ca.pem "$container:/tmp/hhy-ca.pem"
  docker cp build/database-tls/server.pem "$container:/tmp/hhy-server.pem"
  docker cp build/database-tls/server-key.pem "$container:/tmp/hhy-server-key.pem"
done
docker exec -u 0 "$MYSQL_CONTAINER" sh -c 'chown mysql:mysql /tmp/hhy-*.pem; chmod 600 /tmp/hhy-server-key.pem'
docker exec "$MYSQL_CONTAINER" sh -c 'mysql -uroot -p"$MYSQL_ROOT_PASSWORD" -e "SET PERSIST ssl_ca='"'"'/tmp/hhy-ca.pem'"'"'; SET PERSIST ssl_cert='"'"'/tmp/hhy-server.pem'"'"'; SET PERSIST ssl_key='"'"'/tmp/hhy-server-key.pem'"'"'; ALTER INSTANCE RELOAD TLS;"'
docker exec -u 0 "$PG_CONTAINER" sh -c 'chown postgres:postgres /tmp/hhy-*.pem; chmod 600 /tmp/hhy-server-key.pem'
docker exec "$PG_CONTAINER" psql -U hhy_test -d hhy_test -c "ALTER SYSTEM SET ssl_cert_file='/tmp/hhy-server.pem'" -c "ALTER SYSTEM SET ssl_key_file='/tmp/hhy-server-key.pem'" -c "ALTER SYSTEM SET ssl='on'" -c 'SELECT pg_reload_conf()'
