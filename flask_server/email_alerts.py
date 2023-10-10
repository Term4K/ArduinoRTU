import smtplib, ssl

smtp_server = "localhost"
port = 25
sender_email = "alerts@gmail.com"
reciever_email = "steve1985@gmail.com"
message = """\
Subject: Hi there

This message is sent from Python."""
#send an email alert
def send_email_alert(alert_message):
    with smtplib.SMTP(smtp_server, port) as server:
        server.sendmail(sender_email, reciever_email, alert_message)
