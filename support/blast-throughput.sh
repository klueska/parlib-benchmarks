#####################################################################
############################   nginx   ##############################
#####################################################################
mykill() {
	kill -9 $1 2>/dev/null
	wait $! 2>/dev/null
}
launch_blast() {
	./blast 169.229.49.199 80 / -1 100 1000 100 &
	PID=$!
	trap "mykill $PID; exit" SIGHUP SIGINT SIGTERM
}

launch_blast
echo "Num Vcores: $1"
sleep 60
mykill $PID

#####################################################################
####################   linux-upthread-custom   ######################
#####################################################################
#mykill() {
#	kill -9 $1 2>/dev/null
#	wait $! 2>/dev/null
#}
#launch_blast() {
#	./blast 169.229.49.199 8080 / -1 100 1000 100 &
#	PID=$!
#	trap "mykill $PID; exit" SIGHUP SIGINT SIGTERM
#}
#
#launch_blast
#echo "Num Vcores: 2"
#sleep 2
#for i in $(seq 3 32); do 
#	sleep 60
#	OUTPUT=$(curl http://c99.millennium.berkeley.edu:8080/add_vcores?num_vcores=1 2>&1)
#	echo "Num Vcores: $i"
#done
#sleep 60
#mykill $PID

#####################################################################
#######################   linux-upthread   ##########################
#####################################################################
#mykill() {
#	kill -9 $1 2>/dev/null
#	wait $! 2>/dev/null
#}
#launch_blast() {
#	./blast 169.229.49.199 8080 / -1 100 1000 100 &
#	PID=$!
#	trap "mykill $PID; exit" SIGHUP SIGINT SIGTERM
#}
#
#launch_blast
#echo "Num Vcores: 1"
#sleep 2
#for i in $(seq 2 32); do 
#	sleep 60
#	OUTPUT=$(curl http://c99.millennium.berkeley.edu:8080/add_vcores?num_vcores=1 2>&1)
#	echo "Num Vcores: $i"
#done
#sleep 60
#mykill $PID

#####################################################################
########################   native-linux   ###########################
#####################################################################
#mykill() {
#	kill -9 $1 2>/dev/null
#	wait $! 2>/dev/null
#}
#launch_blast() {
#	./blast 169.229.49.199 8080 / -1 100 1000 100 &
#	PID=$!
#	trap "mykill $PID; exit" SIGHUP SIGINT SIGTERM
#}
#
#launch_blast
#echo "Num Vcores: 1"
#for i in $(seq 2  32); do 
#	sleep 60
#	mykill $PID
#	OUTPUT=$(curl http://c99.millennium.berkeley.edu:8080/terminate 2>&1)
#	sleep 10
#	launch_blast
#	echo "Num Vcores: $i"
#done
#sleep 60
#mykill $PID
#OUTPUT=$(curl http://c99.millennium.berkeley.edu:8080/terminate 2>&1)

#####################################################################
####################   akaros-custom-sched   ########################
#####################################################################
#./blast 169.229.49.195 8080 / -1 10 10000 10 &
#PID=$!
#trap "kill -9 $PID; exit" SIGHUP SIGINT SIGTERM
#echo "Num Vcores: 4"
#sleep 2
#for i in $(seq 6 2 32); do 
#	sleep 60
#	OUTPUT=$(curl http://c95.millennium.berkeley.edu:8080/add_vcores?num_vcores=2 2>&1)
#	echo "Num Vcores: $i"
#done
#sleep 60
#kill -9 $PID

#####################################################################
##########################   akaros   ###############################
#####################################################################
#./blast 169.229.49.195 8080 / -1 10 10000 10 &
#PID=$!
#trap "kill -9 $PID; exit" SIGHUP SIGINT SIGTERM
#echo "Num Vcores: 1"
#sleep 2
#for i in $(seq 2 16); do 
#	sleep 60
#	OUTPUT=$(curl http://c95.millennium.berkeley.edu:8080/add_vcores?num_vcores=1 2>&1)
#	echo "Num Vcores: $i"
#done
#sleep 60
#kill -9 $PID

#./blast 169.229.49.195 8080 / -1 10 10000 10 &
#PID=$!
#trap "kill -9 $PID; exit" SIGHUP SIGINT SIGTERM
#OUTPUT=$(curl http://c95.millennium.berkeley.edu:8080/add_vcores?num_vcores=14 2>&1)
#echo "Num Vcores: 15"
#sleep 2
#for i in $(seq 16 16); do 
#	sleep 60
#	OUTPUT=$(curl http://c95.millennium.berkeley.edu:8080/add_vcores?num_vcores=1 2>&1)
#	echo "Num Vcores: $i"
#done
#sleep 60
#kill -9 $PID
