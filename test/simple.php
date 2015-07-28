<?php
binlog_set_position(1751, "MacPro-bin.000005");
$binlog_handler = binlog_connect("mysql://root@127.0.0.1:3306");
while($msg = binlog_wait_for_next_event($binlog_handler)) {
    var_dump($msg);
}
