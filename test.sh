number=0

while true; do

        sleep 1
        number=`expr $number + 1`
        echo ${number} >> number.txt

done
