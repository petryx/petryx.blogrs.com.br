#!/usr/bin/python
# -*- coding: iso-8859-1 -*-
import smtplib
from email.MIMEMultipart import MIMEMultipart
from email.MIMEBase import MIMEBase
from email.MIMEText import MIMEText
from email.Utils import COMMASPACE, formatdate
from email import Encoders
import os
import tarfile
import time
import datetime
import re
 
def sendMail(to, subject, text, files=[],server="localhost"):
    assert type(to)==list
    assert type(files)==list
    fro = "Server Backup <remetente@gmail.com>"
    msg = MIMEMultipart()
    msg['From'] = fro
    msg['To'] = COMMASPACE.join(to)
    msg['Date'] = formatdate(localtime=True)
    today = datetime.date.today()
    msg['Subject'] = subject + str(today)
    msg.attach( MIMEText(text) )
    for file in files:
        part = MIMEBase('application', "octet-stream")
        part.set_payload( open(file,"rb").read() )
        Encoders.encode_base64(part)
        part.add_header('Content-Disposition', 'attachment; filename="%s"'
                      % os.path.basename(file))
        msg.attach(part)
    smtp = smtplib.SMTP(server)
    smtp.sendmail(fro, to, msg.as_string() )
    smtp.close()

def backupTar(nameBackup,conf="/etc/backup"):
    tar = tarfile.open(nameBackup,'w:bz2')
    f = open(conf,'r')
    p = re.compile('\#')
    for line in f:
	    if not p.match(line): #descarta linhas que comecem por #
		file = line.replace('\n','') # Remove \n    
		tar.add(file) #adiciona ou arquivo tar
    tar.close()	

name = 'backupSRV.tar.bz2' #nome do backup
backupTar(name)
sendMail(["destinatario@gmail.com"],"backup","backup",[name])
