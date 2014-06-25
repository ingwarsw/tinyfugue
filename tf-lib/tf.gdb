p "xsock:"
p xsock ? xsock->world->name : "(no xsock)"
p xsock ? xsock->world->host : "(no host)"
p xsock ? xsock->world->port : "(no port)"

p "fsock:"
p fsock ? fsock->world->name : "(no fsock)"
p fsock ? fsock->world->host : "(no host)"
p fsock ? fsock->world->port : "(no port)"

bt full
detach
quit
