
function.o：     文件格式 elf64-x86-64


Disassembly of section .text:

0000000000000000 <add>:
   0:	55                   	push   %rbp
   1:	48 89 e5             	mov    %rsp,%rbp
   4:	f3 0f 11 45 ec       	movss  %xmm0,-0x14(%rbp)
   9:	f3 0f 11 4d e8       	movss  %xmm1,-0x18(%rbp)
   e:	f3 0f 10 45 ec       	movss  -0x14(%rbp),%xmm0
  13:	f3 0f 58 45 e8       	addss  -0x18(%rbp),%xmm0
  18:	f3 0f 11 45 fc       	movss  %xmm0,-0x4(%rbp)
  1d:	8b 45 fc             	mov    -0x4(%rbp),%eax
  20:	89 45 e4             	mov    %eax,-0x1c(%rbp)
  23:	f3 0f 10 45 e4       	movss  -0x1c(%rbp),%xmm0
  28:	c9                   	leave  
  29:	c3                   	ret    

000000000000002a <add_and_multiply>:
  2a:	55                   	push   %rbp
  2b:	48 89 e5             	mov    %rsp,%rbp
  2e:	48 83 ec 20          	sub    $0x20,%rsp
  32:	f3 0f 11 45 ec       	movss  %xmm0,-0x14(%rbp)
  37:	f3 0f 11 4d e8       	movss  %xmm1,-0x18(%rbp)
  3c:	f3 0f 10 4d e8       	movss  -0x18(%rbp),%xmm1
  41:	f3 0f 10 45 ec       	movss  -0x14(%rbp),%xmm0
  46:	e8 00 00 00 00       	call   4b <add_and_multiply+0x21>
  4b:	f3 0f 11 45 fc       	movss  %xmm0,-0x4(%rbp)
  50:	f3 0f 10 4d fc       	movss  -0x4(%rbp),%xmm1
  55:	f3 0f 10 05 00 00 00 	movss  0x0(%rip),%xmm0        # 5d <add_and_multiply+0x33>
  5c:	00 
  5d:	f3 0f 59 c1          	mulss  %xmm1,%xmm0
  61:	f3 0f 11 45 fc       	movss  %xmm0,-0x4(%rbp)
  66:	8b 45 fc             	mov    -0x4(%rbp),%eax
  69:	89 45 e4             	mov    %eax,-0x1c(%rbp)
  6c:	f3 0f 10 45 e4       	movss  -0x1c(%rbp),%xmm0
  71:	c9                   	leave  
  72:	c3                   	ret    

Disassembly of section .bss:

0000000000000000 <nCompletionStatus>:
   0:	00 00                	add    %al,(%rax)
	...

Disassembly of section .rodata:

0000000000000000 <.rodata>:
   0:	00 00                	add    %al,(%rax)
   2:	40                   	rex
   3:	40                   	rex

Disassembly of section .comment:

0000000000000000 <.comment>:
   0:	00 47 43             	add    %al,0x43(%rdi)
   3:	43 3a 20             	rex.XB cmp (%r8),%spl
   6:	28 55 62             	sub    %dl,0x62(%rbp)
   9:	75 6e                	jne    79 <add_and_multiply+0x4f>
   b:	74 75                	je     82 <add_and_multiply+0x58>
   d:	2f                   	(bad)  
   e:	4c 69 6e 61 72 6f 20 	imul   $0x34206f72,0x61(%rsi),%r13
  15:	34 
  16:	2e 34 2e             	cs xor $0x2e,%al
  19:	37                   	(bad)  
  1a:	2d 38 75 62 75       	sub    $0x75627538,%eax
  1f:	6e                   	outsb  %ds:(%rsi),(%dx)
  20:	74 75                	je     97 <add_and_multiply+0x6d>
  22:	31 29                	xor    %ebp,(%rcx)
  24:	20 34 2e             	and    %dh,(%rsi,%rbp,1)
  27:	34 2e                	xor    $0x2e,%al
  29:	37                   	(bad)  
	...

Disassembly of section .eh_frame:

0000000000000000 <.eh_frame>:
   0:	14 00                	adc    $0x0,%al
   2:	00 00                	add    %al,(%rax)
   4:	00 00                	add    %al,(%rax)
   6:	00 00                	add    %al,(%rax)
   8:	01 7a 52             	add    %edi,0x52(%rdx)
   b:	00 01                	add    %al,(%rcx)
   d:	78 10                	js     1f <.eh_frame+0x1f>
   f:	01 1b                	add    %ebx,(%rbx)
  11:	0c 07                	or     $0x7,%al
  13:	08 90 01 00 00 18    	or     %dl,0x18000001(%rax)
  19:	00 00                	add    %al,(%rax)
  1b:	00 1c 00             	add    %bl,(%rax,%rax,1)
  1e:	00 00                	add    %al,(%rax)
  20:	00 00                	add    %al,(%rax)
  22:	00 00                	add    %al,(%rax)
  24:	2a 00                	sub    (%rax),%al
  26:	00 00                	add    %al,(%rax)
  28:	00 41 0e             	add    %al,0xe(%rcx)
  2b:	10 43 86             	adc    %al,-0x7a(%rbx)
  2e:	02 0d 06 00 00 00    	add    0x6(%rip),%cl        # 3a <.eh_frame+0x3a>
  34:	18 00                	sbb    %al,(%rax)
  36:	00 00                	add    %al,(%rax)
  38:	38 00                	cmp    %al,(%rax)
  3a:	00 00                	add    %al,(%rax)
  3c:	00 00                	add    %al,(%rax)
  3e:	00 00                	add    %al,(%rax)
  40:	49 00 00             	rex.WB add %al,(%r8)
  43:	00 00                	add    %al,(%rax)
  45:	41 0e                	rex.B (bad) 
  47:	10 43 86             	adc    %al,-0x7a(%rbx)
  4a:	02 0d 06 00 00 00    	add    0x6(%rip),%cl        # 56 <add_and_multiply+0x2c>
