#!/usr/bin/perl


use     Time::HiRes "usleep";

my 	$cmdline;
my 	$line;
my 	$i;
my 	$result;

$port = $ARGV[0];
if (!length ($port)){
	printf STDERR "usage: DLL13.pl <portname>\n";
	return 1;
}

#
# identity
#
	(<>); # eat spurious char from keyboard
# open first (for read), let harttestframe control handshake

		open (RESPONSE, "<$port")|| die " can't open port\n";

# open port, set handshake, write, reset handshake.....
        $cmdline=("./harttestframe -H $port -S \"ff ff ff ff ff 02 00 00 00\"");               # set supply at 24VDC
        open(SCMD, "$cmdline |");
        close(SCMD);
# let response collect in buffer
		usleep(500000);
# read buffer
		sysread(RESPONSE, $line, 1024, 0);
		close RESPONSE;

		$array = unpack('H*', $line);
		my $len = length($array);
# unpacked
		for($i=0; $i < $len; ){
			$result = $result.substr($array, $i , 2).",";
			$i += 2;
		}
# made string, search it
		$len = length($result);
		for($i = 0; $i < $len; ){
			$val = sprintf  "%02s", substr($result,$i,2);
			if($val eq '06'){
				goto LABEL1;
			}
			$i+= 3;
		}
# found SOH
LABEL1:		

		$address = substr($result,$i+21,6).substr($result,$i + 45 ,9);
#
# now loop for each byte in long request frame
#
		printf "\n\n\n";
		print "DLL013, Found device $address, now test first condition, termination of message1 with 14mS data gap, followed by message2 \n";
		print "DUT should ONLY respond to message 2!!\n\n";

		$message = "ff ff ff ff ff 82 ".$address."00 20 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f 10 11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f 20";
		$message2= "## ff ff ff ff ff 82 ".$address."02 00";
		$mlen = length($message)/3;

		for($loop = 1; $loop < $mlen; $loop++){
			$line = "";
			$result = "";
			$array = "";
	
			open (RESPONSE, "<$port")|| die " can't open port\n";
			$cmdline = sprintf "./harttestframe -H $port -S \"%s%s\" -g $loop " , substr($message,0,$loop * 3), $message2;
			printf "Send to DUT, GAP before byte %02d : $cmdline\n", $loop;
# print location indicator
			for ($sloop = 0; $sloop < (57 + length($port)); $sloop++){
				printf " ";
			}
			for ($sloop = 0 ; $sloop < $loop; $sloop++){
				printf "   ";
			}
			printf "^\n";

   		     open(SCMD, "$cmdline |");
   		     close(SCMD);
# let response collect in buffer
			usleep(500000);
# read buffer
			sysread(RESPONSE, $line, 1024, 0);
			close RESPONSE;

			$array = unpack('H*', $line);
			my $len = length($array);
# unpacked
			for($i=0; $i < $len; ){
				$result = $result.substr($array, $i , 2).",";
				$i += 2;
			}
#made string	
			print	"Received from DUT after waiting : $result \n";
			while((<>) != "\n") {};  # wait from acknowlegde from user (CR)
		}

#
# second test 
#
		printf "\n\n\n";
		print "DLL013 test, now test second condition, 14mS data gap in byte stream \n";
		print "DUT should NOT respond after gap starts at last 2 preample bytes and further!!\n\n";

		$message = "ff ff ff ff ff 82 ".$address."00 20 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f 10 11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f 20";
		$message2= "ff ff ff ff ff 82 ".$address."02 00";
		$mlen = length($message)/3;

		for($loop = 1; $loop < $mlen; $loop++){
			$line = "";
			$result = "";
			$array = "";
	
			open (RESPONSE, "<$port")|| die " can't open port\n";
			$cmdline = sprintf "./harttestframe -H $port -S \"%s\" -g $loop " , $message;
			printf "Send to DUT, GAP before byte %02d : $cmdline\n", $loop;
# print location indicator
			for ($sloop = 0; $sloop < (57 + length($port)); $sloop++){
				printf " ";
			}
			for ($sloop = 0 ; $sloop < $loop; $sloop++){
				printf "   ";
			}
			printf "^\n";


   		     open(SCMD, "$cmdline |");
   		     close(SCMD);
# let response collect in buffer
			usleep(500000);
# read buffer
			sysread(RESPONSE, $line, 1024, 0);
			close RESPONSE;

			$array = unpack('H*', $line);
			my $len = length($array);
# unpacked
			for($i=0; $i < $len; ){
				$result = $result.substr($array, $i , 2).",";
				$i += 2;
			}
			if($len <= 3){
				$result = "NO RESPONSE";
			}
#made string	
			print	"Received from DUT after waiting : $result \n";
			while((<>) != "\n") {};  # wait from acknowlegde from user (CR)
		}

#
# third test
#
		printf "\n\n\n";
		print "DLL013 test, now test third condition, 4mS data gap in byte stream \n";
		print "DUT should ALWAYS respond!\n\n";

		$message = "ff ff ff ff ff 82 ".$address."00 20 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f 10 11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f 20";
		$message2= "ff ff ff ff ff 82 ".$address."02 00";
		$mlen = length($message)/3;

		for($loop = 1; $loop < $mlen; $loop++){
			$line = "";
			$result = "";
			$array = "";
	
			open (RESPONSE, "<$port")|| die " can't open port\n";
			$cmdline = sprintf "./harttestframe -H $port -S \"%s\" -s $loop " , $message;
			printf "Send to DUT, GAP before byte %02d : $cmdline\n", $loop;
# print location indicator
			for ($sloop = 0; $sloop < (57 + length($port)); $sloop++){
				printf " ";
			}
			for ($sloop = 0 ; $sloop < $loop; $sloop++){
				printf "   ";
			}
			printf "^\n";


   		     open(SCMD, "$cmdline |");
   		     close(SCMD);
# let response collect in buffer
			usleep(500000);
# read buffer
			sysread(RESPONSE, $line, 1024, 0);
			close RESPONSE;

			$array = unpack('H*', $line);
			my $len = length($array);
# unpacked
			for($i=0; $i < $len; ){
				$result = $result.substr($array, $i , 2).",";
				$i += 2;
			}
			if($len <= 3){
				$result = "NO RESPONSE";
			}
#made string	
			print	"Received from DUT after waiting : $result \n";
			while((<>) != "\n") {};  # wait from acknowlegde from user (CR)
		}

