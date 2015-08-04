<?php
$handle = binlog_connect("mysql://root@127.0.0.1:3306");
$event = binlog_wait_for_next_event($handle);
binlog_disconnect($handle);
echo "hello, world";
exit();
