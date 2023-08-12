import sys
INPUT_FILE = sys.argv[1]
TIME_INT = int(sys.argv[2])

retx_pre = -1
retx_post = -1
sent_pre = -1
sent_post = -1
recv_pre = -1
recv_post = -1
loss_retx_pre = -1
loss_retx_post = -1

with open(INPUT_FILE) as f1:
    for line in f1:
        line_str = line.split()
        if(line_str[1] =="segments" and line_str[2]=="retransmitted"):
            if(retx_pre == -1):
                retx_pre = int(line_str[0])
                assert(retx_pre != -1)
            else:
                retx_post = int(line_str[0])
        if(line_str[1]=="segments" and line_str[2]=="sent"):
            if(sent_pre == -1):
                sent_pre = int(line_str[0])
                assert(sent_pre != -1)
            else:
                sent_post = int(line_str[0])

        if(line_str[1]=="segments" and line_str[2]=="received"):
            if(recv_pre == -1):
                recv_pre = int(line_str[0])
                assert(recv_pre != -1)
            else:
                recv_post = int(line_str[0])

        if(line_str[0]=="TCPLostRetransmit:"):
            if(loss_retx_pre == -1):
                loss_retx_pre = int(line_str[1])
                assert(loss_retx_pre != -1)
            else:
                loss_retx_post = int(line_str[1])

assert(retx_pre != -1)
assert(retx_post != -1)
assert(sent_pre != -1)
assert(sent_post != -1)
assert(recv_pre != -1)
assert(recv_post != -1)
assert(loss_retx_pre != -1)
assert(loss_retx_post != -1)

retx = float(retx_post - retx_pre)
loss_retx = float(loss_retx_post - loss_retx_pre)
sent = float(sent_post - sent_pre)
recv = float(recv_post - recv_pre)
if(sent > 0):
    retx_rate = retx/sent * 100.0
    loss_retx_rate = loss_retx/sent * 100.0
else:
    retx_rate = 0
    loss_retx_rate = 0
print("Retx: ",retx)
print("Loss_retx: ",loss_retx)
print("Sent: ",sent)
print("Recv: ",recv)
print("Time: ",TIME_INT)
print("Retx_percent: ",retx_rate)
print("Loss_retx_percent: ",loss_retx_rate)
print("Send_rate: ",float(sent)/TIME_INT)
print("Recv_rate: ",float(recv)/TIME_INT)
