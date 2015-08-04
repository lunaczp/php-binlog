<?php
// binlog_set_position(1751, "MacPro-bin.000005");
$binlog_handler = binlog_connect("mysql://root@127.0.0.1:3306");
while($msg = binlog_wait_for_next_event($binlog_handler)) {
    // var_dump($msg);
    // var_dump(binlog_get_position($binlog_handler, $filename));
    // var_dump($filename);
    if(isset($msg["message"])) {
        $a = trim($msg["message"]);
        echo $a . PHP_EOL;
        $m = json_decode($a, true);
        echo json_last_error_msg();
        var_dump($m);
    }
}
