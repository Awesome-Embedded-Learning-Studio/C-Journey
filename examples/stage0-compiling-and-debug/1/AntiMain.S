
main.o：     文件格式 elf64-x86-64


Disassembly of section .text:

0000000000000000 <main>:
   0:	f3 0f 1e fa          	endbr64 
   4:	55                   	push   %rbp
   5:	48 89 e5             	mov    %rsp,%rbp
   8:	48 83 ec 10          	sub    $0x10,%rsp
   c:	f3 0f 10 05 00 00 00 	movss  0x0(%rip),%xmm0        # 14 <main+0x14>
  13:	00 
  14:	f3 0f 11 45 f4       	movss  %xmm0,-0xc(%rbp)
  19:	f3 0f 10 05 00 00 00 	movss  0x0(%rip),%xmm0        # 21 <main+0x21>
  20:	00 
  21:	f3 0f 11 45 f8       	movss  %xmm0,-0x8(%rbp)
  26:	f3 0f 10 45 f8       	movss  -0x8(%rbp),%xmm0
  2b:	8b 45 f4             	mov    -0xc(%rbp),%eax
  2e:	0f 28 c8             	movaps %xmm0,%xmm1
  31:	66 0f 6e c0          	movd   %eax,%xmm0
  35:	e8 00 00 00 00       	call   3a <main+0x3a>
  3a:	66 0f 7e c0          	movd   %xmm0,%eax
  3e:	89 45 fc             	mov    %eax,-0x4(%rbp)
  41:	c7 05 00 00 00 00 01 	movl   $0x1,0x0(%rip)        # 4b <main+0x4b>
  48:	00 00 00 
  4b:	b8 00 00 00 00       	mov    $0x0,%eax
  50:	c9                   	leave  
  51:	c3                   	ret    

Disassembly of section .bss:

0000000000000000 <nCompletionStatus>:
   0:	00 00                	add    %al,(%rax)
	...

Disassembly of section .rodata:

0000000000000000 <.rodata>:
   0:	00 00                	add    %al,(%rax)
   2:	80 3f 00             	cmpb   $0x0,(%rdi)
   5:	00 00                	add    %al,(%rax)
   7:	3f                   	(bad)  

Disassembly of section .comment:

0000000000000000 <.comment>:
   0:	00 47 43             	add    %al,0x43(%rdi)
   3:	43 3a 20             	rex.XB cmp (%r8),%spl
   6:	28 55 62             	sub    %dl,0x62(%rbp)
   9:	75 6e                	jne    79 <main+0x79>
   b:	74 75                	je     82 <main+0x82>
   d:	20 31                	and    %dh,(%rcx)
   f:	31 2e                	xor    %ebp,(%rsi)
  11:	34 2e                	xor    $0x2e,%al
  13:	30 2d 31 75 62 75    	xor    %ch,0x75627531(%rip)        # 7562754a <main+0x7562754a>
  19:	6e                   	outsb  %ds:(%rsi),(%dx)
  1a:	74 75                	je     91 <main+0x91>
  1c:	31 7e 32             	xor    %edi,0x32(%rsi)
  1f:	32 2e                	xor    (%rsi),%ch
  21:	30 34 29             	xor    %dh,(%rcx,%rbp,1)
  24:	20 31                	and    %dh,(%rcx)
  26:	31 2e                	xor    %ebp,(%rsi)
  28:	34 2e                	xor    $0x2e,%al
  2a:	30 00                	xor    %al,(%rax)

Disassembly of section .note.gnu.property:

0000000000000000 <.note.gnu.property>:
   0:	04 00                	add    $0x0,%al
   2:	00 00                	add    %al,(%rax)
   4:	10 00                	adc    %al,(%rax)
   6:	00 00                	add    %al,(%rax)
   8:	05 00 00 00 47       	add    $0x47000000,%eax
   d:	4e 55                	rex.WRX push %rbp
   f:	00 02                	add    %al,(%rdx)
  11:	00 00                	add    %al,(%rax)
  13:	c0 04 00 00          	rolb   $0x0,(%rax,%rax,1)
  17:	00 03                	add    %al,(%rbx)
  19:	00 00                	add    %al,(%rax)
  1b:	00 00                	add    %al,(%rax)
  1d:	00 00                	add    %al,(%rax)
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
  13:	08 90 01 00 00 1c    	or     %dl,0x1c000001(%rax)
  19:	00 00                	add    %al,(%rax)
  1b:	00 1c 00             	add    %bl,(%rax,%rax,1)
  1e:	00 00                	add    %al,(%rax)
  20:	00 00                	add    %al,(%rax)
  22:	00 00                	add    %al,(%rax)
  24:	52                   	push   %rdx
  25:	00 00                	add    %al,(%rax)
  27:	00 00                	add    %al,(%rax)
  29:	45 0e                	rex.RB (bad) 
  2b:	10 86 02 43 0d 06    	adc    %al,0x60d4302(%rsi)
  31:	02 49 0c             	add    0xc(%rcx),%cl
  34:	07                   	(bad)  
  35:	08 00                	or     %al,(%rax)
	...
