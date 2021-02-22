<?php // submit.php
//error_reporting(E_ALL);
//ini_set('display_errors', 1);
date_default_timezone_set("UTC");
$log_file = "/tmp/".basename(__FILE__).'_'.posix_getgid().'.txt';

$old = umask(0);
$fh = fopen($log_file, 'w') or die ("-- Error fopen() --");
umask($old);
fputs($fh, "------- submit.php --------\n");
fflush($fh);

$addr = getenv("SERVER_ADDR");
$port = getenv("SERVER_PORT");

$query = getenv("QUERY_STRING");
$method = getenv("REQUEST_METHOD");
fputs($fh, "REQUEST_METHOD=".$method.";\n");
if(preg_match('/GET/', $method)) {
	if(empty($_GET['firstname'])) $firstname = '?';
	else $firstname = $_GET['firstname'];

	if(empty($_GET['lastname'])) $lastname = '?'; 
	else $lastname = $_GET['lastname'];
}
else if(preg_match('/POST/', $method)) {
	if(empty($query)) $query = 'nil';
	else $firstname = $_POST['firstname'];
	
	if(empty($_POST['firstname'])) $firstname = '?';
	else $firstname = $_POST['firstname'];

	if(empty($_POST['lastname'])) $lastname = '?'; 
	else $lastname = $_POST['lastname'];
}
else {
	$firstname = 'firstname ?';
	$lastname = 'lastname ?';
}

printf("<!DOCTYPE html><html>
   <head>
     <meta charset=\"utf-8\">
     <title>Form Test</title>
   </head>
   <body>
     <p>Здравствуйте, %s %s!</p>
     <form method=\"%s\" action=\"%s\">
       What is your name?<br>
       First name:<input type=\"text\" name=\"firstname\"><br>
       Last name: <input type=\"text\" name=\"lastname\"><br>
       <input type=\"submit\">
     </form>
     <p>query: %s</p>
     <hr noshade>
     %s
   </body>
</html>", $firstname, $lastname, $method, basename(__FILE__), $query, date('r'));
?>
