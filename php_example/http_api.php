<?php

    $myfile = fopen("message.txt", "a");
    fwrite($myfile, json_encode($_POST)."\r\n");
    fclose($myfile);

    echo "success";

?>
