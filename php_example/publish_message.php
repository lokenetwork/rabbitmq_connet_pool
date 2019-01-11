<?php


for( $i=0; $i < 10; $i++ ){
  // 创建 连接 发送消息 接收响应 关闭连接
  $socket = socket_create(AF_UNIX, SOCK_STREAM, 0);
  $bind_file = "/var/tmp/".time().$i;
  socket_bind($socket,$bind_file);
  $resss = socket_connect($socket, '/var/tmp/rabbitmq_connect_pool_server.sock');
  var_dump($resss);
  var_dump($socket);

  $arr['orderno'] = 'leo'.$i;
  $arr['act_id'] = 'luo'.$i;
  $arr['user_id'] = 'luo'.$i;

  $msg = json_encode($arr);

  $body_binarydata = pack("a".strlen($msg), $msg);
  
  $header_binarydata = pack("i", strlen($body_binarydata));

  socket_write ($socket , $header_binarydata, strlen($header_binarydata));

  socket_write ($socket , $body_binarydata, strlen($body_binarydata));

  $result = unpack("i",socket_read($socket,4));
  if( $result[1] ){
    echo "成功\n";
  }else{
    echo "失败\n";
  }
  var_dump($result);
  socket_close($socket);
  unlink($bind_file);
}



?>
