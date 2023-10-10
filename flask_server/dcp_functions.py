#generate the checksum for the DCP message
def DCP_genCmndBCH(buff, count):
    nBCHpoly = 0xB8
    fBCHpoly = 0xFF

    bch = 0
    for i in range(count):
        bch ^= buff[i]

        for j in range(8):
            if (bch & 1) == 1:
                bch = (bch >> 1) ^ nBCHpoly
            else:
                bch >>= 1

    return bch ^ fBCHpoly
#send a DCP message with the BCH checksum
def send_dcp_message(sock, ip_address, address, opcode):
    buff = [address, opcode]
    buff.append(DCP_genCmndBCH(buff, 2))
    sock.sendto(bytes(buff), (ip_address, 8888))
    #print('sent dcp message to ' + ip_address + ' with address ' + str(address) + ' and opcode ' + str(opcode))
