
echo -n "Spawning servers..."
a=1
for i in test1.conf test2.conf test3.conf ; do
	echo -n $a
	./ircd.testbin -f /usr/src/sorcery/sor1_3_3-p1/test/$i
	a=$((a+1))
done
echo ""
echo "Test servers should be running..."
ps axwu |fgrep 'ircd.testbin' |fgrep -v fgrep
echo ""
