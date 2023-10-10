from flask import Flask, render_template, request
import socket
import struct
import threading
from email_alerts import send_email_alert
from dcp_functions import send_dcp_message, DCP_genCmndBCH

host = '192.168.1.1'
data = "No Alerts"
#thread to listen for any alerts from the arduino
def checkAlerts():
    global data
    emailAlerts = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    emailAlerts.bind((host, 8888))

    while True:
        payload, addr = emailAlerts.recvfrom(8888)
        data = payload.decode('utf-8')
        print("received message:", data)
#start the thread to check for alerts constantly
listen_UDP = threading.Thread(target=checkAlerts)
listen_UDP.start()

data = "No Alerts"
last = data

app = Flask(__name__)
#main page of the app
@app.route('/')
def index():
    port = 48996

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((host, port))

    #send dcp message
    send_dcp_message(sock, '192.168.1.177', 0x01, 0x02)
    #initial data to display
    data, server = sock.recvfrom(48996)
    temperature = round(struct.unpack('f', data[1:5])[0], 2)
    dataBuff = struct.unpack('BBBBB', data[0:5])
    print(struct.unpack('BBBBBB', data[0:6]))
    print(temperature)
    print(hex(DCP_genCmndBCH(dataBuff, 5)))
    sock.close()
    return render_template('index.html', temperature=temperature, alarm="No Alerts")

#page to check for any updates to the alerts
@app.route('/alert')
def getAlert():
    global data
    global last
    if data != last:
        send_email_alert("Subject: ALERT PACKET \n" + data)
        last = data
        return data
    return "false"
#get most recent temperature from the arduino
@app.route('/getTemperature')
def getTemperature():
    port = 48996
    #recieve the id/address of the arduino
    arduinoId = int(request.args['id'])
    print(arduinoId)
    #try to get data from the arduino if it is not available return an error message
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind((host, port))
        sock.settimeout(0.1)
        #send dcp message
        send_dcp_message(sock, '192.168.1.177', arduinoId, 0x02)
        data, server = sock.recvfrom(48996)
        temperature = round(struct.unpack('f', data[1:5])[0], 2)
        dataBuff = struct.unpack('BBBBB', data[0:5])

        sock.close()
        return str(temperature)
    except:
        return "No response from Arduino."

if __name__ == '__main__':
    app.run(debug=True)