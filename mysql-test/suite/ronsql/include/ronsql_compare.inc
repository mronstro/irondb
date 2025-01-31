# This include-file, or subroutine, executes a query with both mysql client and
# ronsql_cli, then compares the results.
--source include/have_util_sed.inc
--source include/have_util_curl.inc
--echo == Query ==
if ($QUERY!='')
{
  --echo $QUERY
  --let $QUERY_PARAMS = -e '$QUERY'
  --exec printf '%s' '$QUERY' > $MYSQL_TMP_DIR/ronsql-query.sql
}
if ($QUERY=='')
{
  if ($QUERY_FILE=='')
  {
    --die Must define either QUERY or QUERY_FILE variable
  }
  --exec cat $QUERY_FILE
  --let $QUERY_PARAMS = < '$QUERY_FILE'
  --exec cat $QUERY_FILE > $MYSQL_TMP_DIR/ronsql-query.sql
}
--exec printf '{"query":"' > $MYSQL_TMP_DIR/ronsql-rdrs-body-1
--exec perl -pe 's/\\/\\\\/g; s/"/\\"/g; s/\x08/\\b/g; s/\f/\\f/g; s/\n/\\n/g; s/\r/\\r/g; s/\t/\\t/g;' $MYSQL_TMP_DIR/ronsql-query.sql > $MYSQL_TMP_DIR/ronsql-rdrs-body-2
--exec printf '","database":"test","outputFormat":"TEXT"}' > $MYSQL_TMP_DIR/ronsql-rdrs-body-3
--exec cat $MYSQL_TMP_DIR/ronsql-rdrs-body-1 $MYSQL_TMP_DIR/ronsql-rdrs-body-2 $MYSQL_TMP_DIR/ronsql-rdrs-body-3 > $MYSQL_TMP_DIR/ronsql-rdrs-body
--let $QUERY_PARAMS = -D test $QUERY_PARAMS

--exec $MYSQL --default-character-set=utf8mb4 $QUERY_PARAMS > $MYSQL_TMP_DIR/mysql.out
if ($canonicalization_script)
{
  --exec sed -r '$canonicalization_script' $MYSQL_TMP_DIR/mysql.out > $MYSQL_TMP_DIR/mysql.out.canon
  --move_file $MYSQL_TMP_DIR/mysql.out.canon $MYSQL_TMP_DIR/mysql.out
}
--exec (head -n 1 $MYSQL_TMP_DIR/mysql.out && tail -n +2 $MYSQL_TMP_DIR/mysql.out | sort) > $MYSQL_TMP_DIR/mysql.out.sorted
--echo
--exec echo Number of output lines, including header: `grep -Fc '' '$MYSQL_TMP_DIR/mysql.out.sorted'`

--exec curl --fail-with-body -X POST -H "Content-Type: application/json" http://$RDRS_NOKEY_HOST:$RDRS_NOKEY_PORT/0.1.0/ronsql --data-binary @$MYSQL_TMP_DIR/ronsql-rdrs-body > $MYSQL_TMP_DIR/ronsql-rdrs.out
if ($canonicalization_script)
{
  --exec sed -r '$canonicalization_script' $MYSQL_TMP_DIR/ronsql-rdrs.out > $MYSQL_TMP_DIR/ronsql-rdrs.out.canon
  --move_file $MYSQL_TMP_DIR/ronsql-rdrs.out.canon $MYSQL_TMP_DIR/ronsql-rdrs.out
}
--exec (head -n 1 $MYSQL_TMP_DIR/ronsql-rdrs.out && tail -n +2 $MYSQL_TMP_DIR/ronsql-rdrs.out | sort) > $MYSQL_TMP_DIR/ronsql-rdrs.out.sorted

# todo make cli conditional
--exec $RONSQL_CLI_EXE --connect-string $NDB_CONNECTSTRING $QUERY_PARAMS > $MYSQL_TMP_DIR/ronsql-cli.out
if ($canonicalization_script)
{
  --exec sed -r '$canonicalization_script' $MYSQL_TMP_DIR/ronsql-cli.out > $MYSQL_TMP_DIR/ronsql-cli.out.canon
  --move_file $MYSQL_TMP_DIR/ronsql-cli.out.canon $MYSQL_TMP_DIR/ronsql-cli.out
}
--exec (head -n 1 $MYSQL_TMP_DIR/ronsql-cli.out && tail -n +2 $MYSQL_TMP_DIR/ronsql-cli.out | sort) > $MYSQL_TMP_DIR/ronsql-cli.out.sorted
# todo The "|| true" at the end makes this test succeed no matter what the
# results are. Remove it when possible.
--exec diff -u --label ronsql-cli.out $MYSQL_TMP_DIR/ronsql-cli.out.sorted --label ronsql-rdrs.out $MYSQL_TMP_DIR/ronsql-rdrs.out.sorted || true

--echo
--echo == Diff ==
# todo The "|| true" at the end makes this test succeed no matter what the
# results are. Remove it when possible.
--exec diff -u --label mysql.out $MYSQL_TMP_DIR/mysql.out.sorted --label ronsql.out $MYSQL_TMP_DIR/ronsql-rdrs.out.sorted || true
--remove_file $MYSQL_TMP_DIR/ronsql-query.sql
--remove_file $MYSQL_TMP_DIR/ronsql-rdrs-body
--remove_file $MYSQL_TMP_DIR/ronsql-rdrs-body-1
--remove_file $MYSQL_TMP_DIR/ronsql-rdrs-body-2
--remove_file $MYSQL_TMP_DIR/ronsql-rdrs-body-3
--remove_file $MYSQL_TMP_DIR/mysql.out
--remove_file $MYSQL_TMP_DIR/mysql.out.sorted
--remove_file $MYSQL_TMP_DIR/ronsql-cli.out
--remove_file $MYSQL_TMP_DIR/ronsql-cli.out.sorted
--remove_file $MYSQL_TMP_DIR/ronsql-rdrs.out
--remove_file $MYSQL_TMP_DIR/ronsql-rdrs.out.sorted
--echo ================================================================================
--echo
--echo
--echo
if ($QUERY=='')
{
  --remove_file $QUERY_FILE
}
--let $QUERY=
--let $canonicalization_script=
