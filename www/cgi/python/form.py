#!/usr/bin/python3

import os, datetime 
import cgi
import codecs,sys

#sys.stdout = codecs.getwriter('utf8')(sys.stdout)  не работает

sys.stdout = codecs.getwriter("utf-8")(sys.stdout.detach())
form = cgi.FieldStorage()
text1 = form.getfirst("TEXT_1", "?")
text2 = form.getfirst("TEXT_2", "не задано")


print("Content-Type: text/html\n")

print("""<!DOCTYPE HTML>
<html>
  <head>
   <meta charset="utf-8">
   <title>Обработка данных форм</title>
  </head>
  <body>
   <meta charset=\"utf-8\">""")
print("   <h3>","Здравствуйте ",text1,' ', text2, "!</h3>")
print("   <p>   len=%d</p>"%len(text1))
print("""   <form action=\"/cgi-bin/python/form.py\" method=\"post\" >
    <input type=\"text\" name=\"TEXT_1\" value=\"%s\">
    <input type=\"text\" name=\"TEXT_2\">
    <input type=\"submit\">
   </form>""" %text1)

now = datetime.datetime.utcnow()
print("""   <hr>
   %s
  </body>
</html>""" %now.strftime("%a, %d %b %Y %H:%M:%S GMT"))
