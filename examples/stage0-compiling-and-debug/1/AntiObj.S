
demoApp：     文件格式 elf64-x86-64


Disassembly of section .interp:

0000000000000318 <.interp>:
 318:	2f                   	(bad)  
 319:	6c                   	insb   (%dx),%es:(%rdi)
 31a:	69 62 36 34 2f 6c 64 	imul   $0x646c2f34,0x36(%rdx),%esp
 321:	2d 6c 69 6e 75       	sub    $0x756e696c,%eax
 326:	78 2d                	js     355 <__abi_tag-0x37>
 328:	78 38                	js     362 <__abi_tag-0x2a>
 32a:	36 2d 36 34 2e 73    	ss sub $0x732e3436,%eax
 330:	6f                   	outsl  %ds:(%rsi),(%dx)
 331:	2e 32 00             	cs xor (%rax),%al

Disassembly of section .note.gnu.property:

0000000000000338 <.note.gnu.property>:
 338:	04 00                	add    $0x0,%al
 33a:	00 00                	add    %al,(%rax)
 33c:	20 00                	and    %al,(%rax)
 33e:	00 00                	add    %al,(%rax)
 340:	05 00 00 00 47       	add    $0x47000000,%eax
 345:	4e 55                	rex.WRX push %rbp
 347:	00 02                	add    %al,(%rdx)
 349:	00 00                	add    %al,(%rax)
 34b:	c0 04 00 00          	rolb   $0x0,(%rax,%rax,1)
 34f:	00 03                	add    %al,(%rbx)
 351:	00 00                	add    %al,(%rax)
 353:	00 00                	add    %al,(%rax)
 355:	00 00                	add    %al,(%rax)
 357:	00 02                	add    %al,(%rdx)
 359:	80 00 c0             	addb   $0xc0,(%rax)
 35c:	04 00                	add    $0x0,%al
 35e:	00 00                	add    %al,(%rax)
 360:	01 00                	add    %eax,(%rax)
 362:	00 00                	add    %al,(%rax)
 364:	00 00                	add    %al,(%rax)
	...

Disassembly of section .note.gnu.build-id:

0000000000000368 <.note.gnu.build-id>:
 368:	04 00                	add    $0x0,%al
 36a:	00 00                	add    %al,(%rax)
 36c:	14 00                	adc    $0x0,%al
 36e:	00 00                	add    %al,(%rax)
 370:	03 00                	add    (%rax),%eax
 372:	00 00                	add    %al,(%rax)
 374:	47                   	rex.RXB
 375:	4e 55                	rex.WRX push %rbp
 377:	00 9d 9e 18 65 de    	add    %bl,-0x219ae762(%rbp)
 37d:	63 b4 00 b4 36 39 b1 	movsxd -0x4ec6c94c(%rax,%rax,1),%esi
 384:	dd d2                	fst    %st(2)
 386:	40 5d                	rex pop %rbp
 388:	cf                   	iret   
 389:	7a 6c                	jp     3f7 <__abi_tag+0x6b>
 38b:	08                   	.byte 0x8

Disassembly of section .note.ABI-tag:

000000000000038c <__abi_tag>:
 38c:	04 00                	add    $0x0,%al
 38e:	00 00                	add    %al,(%rax)
 390:	10 00                	adc    %al,(%rax)
 392:	00 00                	add    %al,(%rax)
 394:	01 00                	add    %eax,(%rax)
 396:	00 00                	add    %al,(%rax)
 398:	47                   	rex.RXB
 399:	4e 55                	rex.WRX push %rbp
 39b:	00 00                	add    %al,(%rax)
 39d:	00 00                	add    %al,(%rax)
 39f:	00 03                	add    %al,(%rbx)
 3a1:	00 00                	add    %al,(%rax)
 3a3:	00 02                	add    %al,(%rdx)
 3a5:	00 00                	add    %al,(%rax)
 3a7:	00 00                	add    %al,(%rax)
 3a9:	00 00                	add    %al,(%rax)
	...

Disassembly of section .gnu.hash:

00000000000003b0 <.gnu.hash>:
 3b0:	02 00                	add    (%rax),%al
 3b2:	00 00                	add    %al,(%rax)
 3b4:	05 00 00 00 01       	add    $0x1000000,%eax
 3b9:	00 00                	add    %al,(%rax)
 3bb:	00 06                	add    %al,(%rsi)
 3bd:	00 00                	add    %al,(%rax)
 3bf:	00 00                	add    %al,(%rax)
 3c1:	00 81 00 00 00 00    	add    %al,0x0(%rcx)
 3c7:	00 05 00 00 00 00    	add    %al,0x0(%rip)        # 3cd <__abi_tag+0x41>
 3cd:	00 00                	add    %al,(%rax)
 3cf:	00 d1                	add    %dl,%cl
 3d1:	65 ce                	gs (bad) 
 3d3:	6d                   	insl   (%dx),%es:(%rdi)

Disassembly of section .dynsym:

00000000000003d8 <.dynsym>:
	...
 3f0:	10 00                	adc    %al,(%rax)
 3f2:	00 00                	add    %al,(%rax)
 3f4:	12 00                	adc    (%rax),%al
	...
 406:	00 00                	add    %al,(%rax)
 408:	43 00 00             	rex.XB add %al,(%r8)
 40b:	00 20                	add    %ah,(%rax)
	...
 41d:	00 00                	add    %al,(%rax)
 41f:	00 5f 00             	add    %bl,0x0(%rdi)
 422:	00 00                	add    %al,(%rax)
 424:	20 00                	and    %al,(%rax)
	...
 436:	00 00                	add    %al,(%rax)
 438:	6e                   	outsb  %ds:(%rsi),(%dx)
 439:	00 00                	add    %al,(%rax)
 43b:	00 20                	add    %ah,(%rax)
	...
 44d:	00 00                	add    %al,(%rax)
 44f:	00 01                	add    %al,(%rcx)
 451:	00 00                	add    %al,(%rax)
 453:	00 22                	add    %ah,(%rdx)
	...

Disassembly of section .dynstr:

0000000000000468 <.dynstr>:
 468:	00 5f 5f             	add    %bl,0x5f(%rdi)
 46b:	63 78 61             	movsxd 0x61(%rax),%edi
 46e:	5f                   	pop    %rdi
 46f:	66 69 6e 61 6c 69    	imul   $0x696c,0x61(%rsi),%bp
 475:	7a 65                	jp     4dc <__abi_tag+0x150>
 477:	00 5f 5f             	add    %bl,0x5f(%rdi)
 47a:	6c                   	insb   (%dx),%es:(%rdi)
 47b:	69 62 63 5f 73 74 61 	imul   $0x6174735f,0x63(%rdx),%esp
 482:	72 74                	jb     4f8 <__abi_tag+0x16c>
 484:	5f                   	pop    %rdi
 485:	6d                   	insl   (%dx),%es:(%rdi)
 486:	61                   	(bad)  
 487:	69 6e 00 6c 69 62 63 	imul   $0x6362696c,0x0(%rsi),%ebp
 48e:	2e 73 6f             	jae,pn 500 <__abi_tag+0x174>
 491:	2e 36 00 47 4c       	cs ss add %al,0x4c(%rdi)
 496:	49                   	rex.WB
 497:	42                   	rex.X
 498:	43 5f                	rex.XB pop %r15
 49a:	32 2e                	xor    (%rsi),%ch
 49c:	32 2e                	xor    (%rsi),%ch
 49e:	35 00 47 4c 49       	xor    $0x494c4700,%eax
 4a3:	42                   	rex.X
 4a4:	43 5f                	rex.XB pop %r15
 4a6:	32 2e                	xor    (%rsi),%ch
 4a8:	33 34 00             	xor    (%rax,%rax,1),%esi
 4ab:	5f                   	pop    %rdi
 4ac:	49 54                	rex.WB push %r12
 4ae:	4d 5f                	rex.WRB pop %r15
 4b0:	64 65 72 65          	fs gs jb 519 <__abi_tag+0x18d>
 4b4:	67 69 73 74 65 72 54 	imul   $0x4d547265,0x74(%ebx),%esi
 4bb:	4d 
 4bc:	43 6c                	rex.XB insb (%dx),%es:(%rdi)
 4be:	6f                   	outsl  %ds:(%rsi),(%dx)
 4bf:	6e                   	outsb  %ds:(%rsi),(%dx)
 4c0:	65 54                	gs push %rsp
 4c2:	61                   	(bad)  
 4c3:	62                   	(bad)  
 4c4:	6c                   	insb   (%dx),%es:(%rdi)
 4c5:	65 00 5f 5f          	add    %bl,%gs:0x5f(%rdi)
 4c9:	67 6d                	insl   (%dx),%es:(%edi)
 4cb:	6f                   	outsl  %ds:(%rsi),(%dx)
 4cc:	6e                   	outsb  %ds:(%rsi),(%dx)
 4cd:	5f                   	pop    %rdi
 4ce:	73 74                	jae    544 <__abi_tag+0x1b8>
 4d0:	61                   	(bad)  
 4d1:	72 74                	jb     547 <__abi_tag+0x1bb>
 4d3:	5f                   	pop    %rdi
 4d4:	5f                   	pop    %rdi
 4d5:	00 5f 49             	add    %bl,0x49(%rdi)
 4d8:	54                   	push   %rsp
 4d9:	4d 5f                	rex.WRB pop %r15
 4db:	72 65                	jb     542 <__abi_tag+0x1b6>
 4dd:	67 69 73 74 65 72 54 	imul   $0x4d547265,0x74(%ebx),%esi
 4e4:	4d 
 4e5:	43 6c                	rex.XB insb (%dx),%es:(%rdi)
 4e7:	6f                   	outsl  %ds:(%rsi),(%dx)
 4e8:	6e                   	outsb  %ds:(%rsi),(%dx)
 4e9:	65 54                	gs push %rsp
 4eb:	61                   	(bad)  
 4ec:	62                   	.byte 0x62
 4ed:	6c                   	insb   (%dx),%es:(%rdi)
 4ee:	65                   	gs
	...

Disassembly of section .gnu.version:

00000000000004f0 <.gnu.version>:
 4f0:	00 00                	add    %al,(%rax)
 4f2:	02 00                	add    (%rax),%al
 4f4:	01 00                	add    %eax,(%rax)
 4f6:	01 00                	add    %eax,(%rax)
 4f8:	01 00                	add    %eax,(%rax)
 4fa:	03 00                	add    (%rax),%eax

Disassembly of section .gnu.version_r:

0000000000000500 <.gnu.version_r>:
 500:	01 00                	add    %eax,(%rax)
 502:	02 00                	add    (%rax),%al
 504:	22 00                	and    (%rax),%al
 506:	00 00                	add    %al,(%rax)
 508:	10 00                	adc    %al,(%rax)
 50a:	00 00                	add    %al,(%rax)
 50c:	00 00                	add    %al,(%rax)
 50e:	00 00                	add    %al,(%rax)
 510:	75 1a                	jne    52c <__abi_tag+0x1a0>
 512:	69 09 00 00 03 00    	imul   $0x30000,(%rcx),%ecx
 518:	2c 00                	sub    $0x0,%al
 51a:	00 00                	add    %al,(%rax)
 51c:	10 00                	adc    %al,(%rax)
 51e:	00 00                	add    %al,(%rax)
 520:	b4 91                	mov    $0x91,%ah
 522:	96                   	xchg   %eax,%esi
 523:	06                   	(bad)  
 524:	00 00                	add    %al,(%rax)
 526:	02 00                	add    (%rax),%al
 528:	38 00                	cmp    %al,(%rax)
 52a:	00 00                	add    %al,(%rax)
 52c:	00 00                	add    %al,(%rax)
	...

Disassembly of section .rela.dyn:

0000000000000530 <.rela.dyn>:
 530:	f0 3d 00 00 00 00    	lock cmp $0x0,%eax
 536:	00 00                	add    %al,(%rax)
 538:	08 00                	or     %al,(%rax)
 53a:	00 00                	add    %al,(%rax)
 53c:	00 00                	add    %al,(%rax)
 53e:	00 00                	add    %al,(%rax)
 540:	20 11                	and    %dl,(%rcx)
 542:	00 00                	add    %al,(%rax)
 544:	00 00                	add    %al,(%rax)
 546:	00 00                	add    %al,(%rax)
 548:	f8                   	clc    
 549:	3d 00 00 00 00       	cmp    $0x0,%eax
 54e:	00 00                	add    %al,(%rax)
 550:	08 00                	or     %al,(%rax)
 552:	00 00                	add    %al,(%rax)
 554:	00 00                	add    %al,(%rax)
 556:	00 00                	add    %al,(%rax)
 558:	e0 10                	loopne 56a <__abi_tag+0x1de>
 55a:	00 00                	add    %al,(%rax)
 55c:	00 00                	add    %al,(%rax)
 55e:	00 00                	add    %al,(%rax)
 560:	08 40 00             	or     %al,0x0(%rax)
 563:	00 00                	add    %al,(%rax)
 565:	00 00                	add    %al,(%rax)
 567:	00 08                	add    %cl,(%rax)
 569:	00 00                	add    %al,(%rax)
 56b:	00 00                	add    %al,(%rax)
 56d:	00 00                	add    %al,(%rax)
 56f:	00 08                	add    %cl,(%rax)
 571:	40 00 00             	rex add %al,(%rax)
 574:	00 00                	add    %al,(%rax)
 576:	00 00                	add    %al,(%rax)
 578:	d8 3f                	fdivrs (%rdi)
 57a:	00 00                	add    %al,(%rax)
 57c:	00 00                	add    %al,(%rax)
 57e:	00 00                	add    %al,(%rax)
 580:	06                   	(bad)  
 581:	00 00                	add    %al,(%rax)
 583:	00 01                	add    %al,(%rcx)
	...
 58d:	00 00                	add    %al,(%rax)
 58f:	00 e0                	add    %ah,%al
 591:	3f                   	(bad)  
 592:	00 00                	add    %al,(%rax)
 594:	00 00                	add    %al,(%rax)
 596:	00 00                	add    %al,(%rax)
 598:	06                   	(bad)  
 599:	00 00                	add    %al,(%rax)
 59b:	00 02                	add    %al,(%rdx)
	...
 5a5:	00 00                	add    %al,(%rax)
 5a7:	00 e8                	add    %ch,%al
 5a9:	3f                   	(bad)  
 5aa:	00 00                	add    %al,(%rax)
 5ac:	00 00                	add    %al,(%rax)
 5ae:	00 00                	add    %al,(%rax)
 5b0:	06                   	(bad)  
 5b1:	00 00                	add    %al,(%rax)
 5b3:	00 03                	add    %al,(%rbx)
	...
 5bd:	00 00                	add    %al,(%rax)
 5bf:	00 f0                	add    %dh,%al
 5c1:	3f                   	(bad)  
 5c2:	00 00                	add    %al,(%rax)
 5c4:	00 00                	add    %al,(%rax)
 5c6:	00 00                	add    %al,(%rax)
 5c8:	06                   	(bad)  
 5c9:	00 00                	add    %al,(%rax)
 5cb:	00 04 00             	add    %al,(%rax,%rax,1)
	...
 5d6:	00 00                	add    %al,(%rax)
 5d8:	f8                   	clc    
 5d9:	3f                   	(bad)  
 5da:	00 00                	add    %al,(%rax)
 5dc:	00 00                	add    %al,(%rax)
 5de:	00 00                	add    %al,(%rax)
 5e0:	06                   	(bad)  
 5e1:	00 00                	add    %al,(%rax)
 5e3:	00 05 00 00 00 00    	add    %al,0x0(%rip)        # 5e9 <__abi_tag+0x25d>
 5e9:	00 00                	add    %al,(%rax)
 5eb:	00 00                	add    %al,(%rax)
 5ed:	00 00                	add    %al,(%rax)
	...

Disassembly of section .init:

0000000000001000 <_init>:
    1000:	f3 0f 1e fa          	endbr64 
    1004:	48 83 ec 08          	sub    $0x8,%rsp
    1008:	48 8b 05 d9 2f 00 00 	mov    0x2fd9(%rip),%rax        # 3fe8 <__gmon_start__@Base>
    100f:	48 85 c0             	test   %rax,%rax
    1012:	74 02                	je     1016 <_init+0x16>
    1014:	ff d0                	call   *%rax
    1016:	48 83 c4 08          	add    $0x8,%rsp
    101a:	c3                   	ret    

Disassembly of section .plt:

0000000000001020 <.plt>:
    1020:	ff 35 a2 2f 00 00    	push   0x2fa2(%rip)        # 3fc8 <_GLOBAL_OFFSET_TABLE_+0x8>
    1026:	f2 ff 25 a3 2f 00 00 	bnd jmp *0x2fa3(%rip)        # 3fd0 <_GLOBAL_OFFSET_TABLE_+0x10>
    102d:	0f 1f 00             	nopl   (%rax)

Disassembly of section .plt.got:

0000000000001030 <__cxa_finalize@plt>:
    1030:	f3 0f 1e fa          	endbr64 
    1034:	f2 ff 25 bd 2f 00 00 	bnd jmp *0x2fbd(%rip)        # 3ff8 <__cxa_finalize@GLIBC_2.2.5>
    103b:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)

Disassembly of section .text:

0000000000001040 <_start>:
    1040:	f3 0f 1e fa          	endbr64 
    1044:	31 ed                	xor    %ebp,%ebp
    1046:	49 89 d1             	mov    %rdx,%r9
    1049:	5e                   	pop    %rsi
    104a:	48 89 e2             	mov    %rsp,%rdx
    104d:	48 83 e4 f0          	and    $0xfffffffffffffff0,%rsp
    1051:	50                   	push   %rax
    1052:	54                   	push   %rsp
    1053:	45 31 c0             	xor    %r8d,%r8d
    1056:	31 c9                	xor    %ecx,%ecx
    1058:	48 8d 3d 40 01 00 00 	lea    0x140(%rip),%rdi        # 119f <main>
    105f:	ff 15 73 2f 00 00    	call   *0x2f73(%rip)        # 3fd8 <__libc_start_main@GLIBC_2.34>
    1065:	f4                   	hlt    
    1066:	66 2e 0f 1f 84 00 00 	cs nopw 0x0(%rax,%rax,1)
    106d:	00 00 00 

0000000000001070 <deregister_tm_clones>:
    1070:	48 8d 3d 99 2f 00 00 	lea    0x2f99(%rip),%rdi        # 4010 <__TMC_END__>
    1077:	48 8d 05 92 2f 00 00 	lea    0x2f92(%rip),%rax        # 4010 <__TMC_END__>
    107e:	48 39 f8             	cmp    %rdi,%rax
    1081:	74 15                	je     1098 <deregister_tm_clones+0x28>
    1083:	48 8b 05 56 2f 00 00 	mov    0x2f56(%rip),%rax        # 3fe0 <_ITM_deregisterTMCloneTable@Base>
    108a:	48 85 c0             	test   %rax,%rax
    108d:	74 09                	je     1098 <deregister_tm_clones+0x28>
    108f:	ff e0                	jmp    *%rax
    1091:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)
    1098:	c3                   	ret    
    1099:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)

00000000000010a0 <register_tm_clones>:
    10a0:	48 8d 3d 69 2f 00 00 	lea    0x2f69(%rip),%rdi        # 4010 <__TMC_END__>
    10a7:	48 8d 35 62 2f 00 00 	lea    0x2f62(%rip),%rsi        # 4010 <__TMC_END__>
    10ae:	48 29 fe             	sub    %rdi,%rsi
    10b1:	48 89 f0             	mov    %rsi,%rax
    10b4:	48 c1 ee 3f          	shr    $0x3f,%rsi
    10b8:	48 c1 f8 03          	sar    $0x3,%rax
    10bc:	48 01 c6             	add    %rax,%rsi
    10bf:	48 d1 fe             	sar    %rsi
    10c2:	74 14                	je     10d8 <register_tm_clones+0x38>
    10c4:	48 8b 05 25 2f 00 00 	mov    0x2f25(%rip),%rax        # 3ff0 <_ITM_registerTMCloneTable@Base>
    10cb:	48 85 c0             	test   %rax,%rax
    10ce:	74 08                	je     10d8 <register_tm_clones+0x38>
    10d0:	ff e0                	jmp    *%rax
    10d2:	66 0f 1f 44 00 00    	nopw   0x0(%rax,%rax,1)
    10d8:	c3                   	ret    
    10d9:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)

00000000000010e0 <__do_global_dtors_aux>:
    10e0:	f3 0f 1e fa          	endbr64 
    10e4:	80 3d 25 2f 00 00 00 	cmpb   $0x0,0x2f25(%rip)        # 4010 <__TMC_END__>
    10eb:	75 2b                	jne    1118 <__do_global_dtors_aux+0x38>
    10ed:	55                   	push   %rbp
    10ee:	48 83 3d 02 2f 00 00 	cmpq   $0x0,0x2f02(%rip)        # 3ff8 <__cxa_finalize@GLIBC_2.2.5>
    10f5:	00 
    10f6:	48 89 e5             	mov    %rsp,%rbp
    10f9:	74 0c                	je     1107 <__do_global_dtors_aux+0x27>
    10fb:	48 8b 3d 06 2f 00 00 	mov    0x2f06(%rip),%rdi        # 4008 <__dso_handle>
    1102:	e8 29 ff ff ff       	call   1030 <__cxa_finalize@plt>
    1107:	e8 64 ff ff ff       	call   1070 <deregister_tm_clones>
    110c:	c6 05 fd 2e 00 00 01 	movb   $0x1,0x2efd(%rip)        # 4010 <__TMC_END__>
    1113:	5d                   	pop    %rbp
    1114:	c3                   	ret    
    1115:	0f 1f 00             	nopl   (%rax)
    1118:	c3                   	ret    
    1119:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)

0000000000001120 <frame_dummy>:
    1120:	f3 0f 1e fa          	endbr64 
    1124:	e9 77 ff ff ff       	jmp    10a0 <register_tm_clones>

0000000000001129 <add>:
    1129:	f3 0f 1e fa          	endbr64 
    112d:	55                   	push   %rbp
    112e:	48 89 e5             	mov    %rsp,%rbp
    1131:	f3 0f 11 45 ec       	movss  %xmm0,-0x14(%rbp)
    1136:	f3 0f 11 4d e8       	movss  %xmm1,-0x18(%rbp)
    113b:	f3 0f 10 45 ec       	movss  -0x14(%rbp),%xmm0
    1140:	f3 0f 58 45 e8       	addss  -0x18(%rbp),%xmm0
    1145:	f3 0f 11 45 fc       	movss  %xmm0,-0x4(%rbp)
    114a:	f3 0f 10 45 fc       	movss  -0x4(%rbp),%xmm0
    114f:	5d                   	pop    %rbp
    1150:	c3                   	ret    

0000000000001151 <add_and_multiply>:
    1151:	f3 0f 1e fa          	endbr64 
    1155:	55                   	push   %rbp
    1156:	48 89 e5             	mov    %rsp,%rbp
    1159:	48 83 ec 18          	sub    $0x18,%rsp
    115d:	f3 0f 11 45 ec       	movss  %xmm0,-0x14(%rbp)
    1162:	f3 0f 11 4d e8       	movss  %xmm1,-0x18(%rbp)
    1167:	f3 0f 10 45 e8       	movss  -0x18(%rbp),%xmm0
    116c:	8b 45 ec             	mov    -0x14(%rbp),%eax
    116f:	0f 28 c8             	movaps %xmm0,%xmm1
    1172:	66 0f 6e c0          	movd   %eax,%xmm0
    1176:	e8 ae ff ff ff       	call   1129 <add>
    117b:	66 0f 7e c0          	movd   %xmm0,%eax
    117f:	89 45 fc             	mov    %eax,-0x4(%rbp)
    1182:	f3 0f 10 4d fc       	movss  -0x4(%rbp),%xmm1
    1187:	f3 0f 10 05 75 0e 00 	movss  0xe75(%rip),%xmm0        # 2004 <_IO_stdin_used+0x4>
    118e:	00 
    118f:	f3 0f 59 c1          	mulss  %xmm1,%xmm0
    1193:	f3 0f 11 45 fc       	movss  %xmm0,-0x4(%rbp)
    1198:	f3 0f 10 45 fc       	movss  -0x4(%rbp),%xmm0
    119d:	c9                   	leave  
    119e:	c3                   	ret    

000000000000119f <main>:
    119f:	f3 0f 1e fa          	endbr64 
    11a3:	55                   	push   %rbp
    11a4:	48 89 e5             	mov    %rsp,%rbp
    11a7:	48 83 ec 10          	sub    $0x10,%rsp
    11ab:	f3 0f 10 05 55 0e 00 	movss  0xe55(%rip),%xmm0        # 2008 <_IO_stdin_used+0x8>
    11b2:	00 
    11b3:	f3 0f 11 45 f4       	movss  %xmm0,-0xc(%rbp)
    11b8:	f3 0f 10 05 4c 0e 00 	movss  0xe4c(%rip),%xmm0        # 200c <_IO_stdin_used+0xc>
    11bf:	00 
    11c0:	f3 0f 11 45 f8       	movss  %xmm0,-0x8(%rbp)
    11c5:	f3 0f 10 45 f8       	movss  -0x8(%rbp),%xmm0
    11ca:	8b 45 f4             	mov    -0xc(%rbp),%eax
    11cd:	0f 28 c8             	movaps %xmm0,%xmm1
    11d0:	66 0f 6e c0          	movd   %eax,%xmm0
    11d4:	e8 78 ff ff ff       	call   1151 <add_and_multiply>
    11d9:	66 0f 7e c0          	movd   %xmm0,%eax
    11dd:	89 45 fc             	mov    %eax,-0x4(%rbp)
    11e0:	c7 05 2a 2e 00 00 01 	movl   $0x1,0x2e2a(%rip)        # 4014 <nCompletionStatus>
    11e7:	00 00 00 
    11ea:	b8 00 00 00 00       	mov    $0x0,%eax
    11ef:	c9                   	leave  
    11f0:	c3                   	ret    

Disassembly of section .fini:

00000000000011f4 <_fini>:
    11f4:	f3 0f 1e fa          	endbr64 
    11f8:	48 83 ec 08          	sub    $0x8,%rsp
    11fc:	48 83 c4 08          	add    $0x8,%rsp
    1200:	c3                   	ret    

Disassembly of section .rodata:

0000000000002000 <_IO_stdin_used>:
    2000:	01 00                	add    %eax,(%rax)
    2002:	02 00                	add    (%rax),%al
    2004:	00 00                	add    %al,(%rax)
    2006:	40                   	rex
    2007:	40 00 00             	rex add %al,(%rax)
    200a:	80 3f 00             	cmpb   $0x0,(%rdi)
    200d:	00 00                	add    %al,(%rax)
    200f:	3f                   	(bad)  

Disassembly of section .eh_frame_hdr:

0000000000002010 <__GNU_EH_FRAME_HDR>:
    2010:	01 1b                	add    %ebx,(%rbx)
    2012:	03 3b                	add    (%rbx),%edi
    2014:	3c 00                	cmp    $0x0,%al
    2016:	00 00                	add    %al,(%rax)
    2018:	06                   	(bad)  
    2019:	00 00                	add    %al,(%rax)
    201b:	00 10                	add    %dl,(%rax)
    201d:	f0 ff                	lock (bad) 
    201f:	ff 70 00             	push   0x0(%rax)
    2022:	00 00                	add    %al,(%rax)
    2024:	20 f0                	and    %dh,%al
    2026:	ff                   	(bad)  
    2027:	ff 98 00 00 00 30    	lcall  *0x30000000(%rax)
    202d:	f0 ff                	lock (bad) 
    202f:	ff 58 00             	lcall  *0x0(%rax)
    2032:	00 00                	add    %al,(%rax)
    2034:	19 f1                	sbb    %esi,%ecx
    2036:	ff                   	(bad)  
    2037:	ff b0 00 00 00 41    	push   0x41000000(%rax)
    203d:	f1                   	int1   
    203e:	ff                   	(bad)  
    203f:	ff d0                	call   *%rax
    2041:	00 00                	add    %al,(%rax)
    2043:	00 8f f1 ff ff f0    	add    %cl,-0xf00000f(%rdi)
    2049:	00 00                	add    %al,(%rax)
	...

Disassembly of section .eh_frame:

0000000000002050 <__FRAME_END__-0xd0>:
    2050:	14 00                	adc    $0x0,%al
    2052:	00 00                	add    %al,(%rax)
    2054:	00 00                	add    %al,(%rax)
    2056:	00 00                	add    %al,(%rax)
    2058:	01 7a 52             	add    %edi,0x52(%rdx)
    205b:	00 01                	add    %al,(%rcx)
    205d:	78 10                	js     206f <__GNU_EH_FRAME_HDR+0x5f>
    205f:	01 1b                	add    %ebx,(%rbx)
    2061:	0c 07                	or     $0x7,%al
    2063:	08 90 01 00 00 14    	or     %dl,0x14000001(%rax)
    2069:	00 00                	add    %al,(%rax)
    206b:	00 1c 00             	add    %bl,(%rax,%rax,1)
    206e:	00 00                	add    %al,(%rax)
    2070:	d0 ef                	shr    %bh
    2072:	ff                   	(bad)  
    2073:	ff 26                	jmp    *(%rsi)
    2075:	00 00                	add    %al,(%rax)
    2077:	00 00                	add    %al,(%rax)
    2079:	44 07                	rex.R (bad) 
    207b:	10 00                	adc    %al,(%rax)
    207d:	00 00                	add    %al,(%rax)
    207f:	00 24 00             	add    %ah,(%rax,%rax,1)
    2082:	00 00                	add    %al,(%rax)
    2084:	34 00                	xor    $0x0,%al
    2086:	00 00                	add    %al,(%rax)
    2088:	98                   	cwtl   
    2089:	ef                   	out    %eax,(%dx)
    208a:	ff                   	(bad)  
    208b:	ff 10                	call   *(%rax)
    208d:	00 00                	add    %al,(%rax)
    208f:	00 00                	add    %al,(%rax)
    2091:	0e                   	(bad)  
    2092:	10 46 0e             	adc    %al,0xe(%rsi)
    2095:	18 4a 0f             	sbb    %cl,0xf(%rdx)
    2098:	0b 77 08             	or     0x8(%rdi),%esi
    209b:	80 00 3f             	addb   $0x3f,(%rax)
    209e:	1a 3a                	sbb    (%rdx),%bh
    20a0:	2a 33                	sub    (%rbx),%dh
    20a2:	24 22                	and    $0x22,%al
    20a4:	00 00                	add    %al,(%rax)
    20a6:	00 00                	add    %al,(%rax)
    20a8:	14 00                	adc    $0x0,%al
    20aa:	00 00                	add    %al,(%rax)
    20ac:	5c                   	pop    %rsp
    20ad:	00 00                	add    %al,(%rax)
    20af:	00 80 ef ff ff 10    	add    %al,0x10ffffef(%rax)
	...
    20bd:	00 00                	add    %al,(%rax)
    20bf:	00 1c 00             	add    %bl,(%rax,%rax,1)
    20c2:	00 00                	add    %al,(%rax)
    20c4:	74 00                	je     20c6 <__GNU_EH_FRAME_HDR+0xb6>
    20c6:	00 00                	add    %al,(%rax)
    20c8:	61                   	(bad)  
    20c9:	f0 ff                	lock (bad) 
    20cb:	ff 28                	ljmp   *(%rax)
    20cd:	00 00                	add    %al,(%rax)
    20cf:	00 00                	add    %al,(%rax)
    20d1:	45 0e                	rex.RB (bad) 
    20d3:	10 86 02 43 0d 06    	adc    %al,0x60d4302(%rsi)
    20d9:	5f                   	pop    %rdi
    20da:	0c 07                	or     $0x7,%al
    20dc:	08 00                	or     %al,(%rax)
    20de:	00 00                	add    %al,(%rax)
    20e0:	1c 00                	sbb    $0x0,%al
    20e2:	00 00                	add    %al,(%rax)
    20e4:	94                   	xchg   %eax,%esp
    20e5:	00 00                	add    %al,(%rax)
    20e7:	00 69 f0             	add    %ch,-0x10(%rcx)
    20ea:	ff                   	(bad)  
    20eb:	ff 4e 00             	decl   0x0(%rsi)
    20ee:	00 00                	add    %al,(%rax)
    20f0:	00 45 0e             	add    %al,0xe(%rbp)
    20f3:	10 86 02 43 0d 06    	adc    %al,0x60d4302(%rsi)
    20f9:	02 45 0c             	add    0xc(%rbp),%al
    20fc:	07                   	(bad)  
    20fd:	08 00                	or     %al,(%rax)
    20ff:	00 1c 00             	add    %bl,(%rax,%rax,1)
    2102:	00 00                	add    %al,(%rax)
    2104:	b4 00                	mov    $0x0,%ah
    2106:	00 00                	add    %al,(%rax)
    2108:	97                   	xchg   %eax,%edi
    2109:	f0 ff                	lock (bad) 
    210b:	ff 52 00             	call   *0x0(%rdx)
    210e:	00 00                	add    %al,(%rax)
    2110:	00 45 0e             	add    %al,0xe(%rbp)
    2113:	10 86 02 43 0d 06    	adc    %al,0x60d4302(%rsi)
    2119:	02 49 0c             	add    0xc(%rcx),%cl
    211c:	07                   	(bad)  
    211d:	08 00                	or     %al,(%rax)
	...

0000000000002120 <__FRAME_END__>:
    2120:	00 00                	add    %al,(%rax)
	...

Disassembly of section .init_array:

0000000000003df0 <__frame_dummy_init_array_entry>:
    3df0:	20 11                	and    %dl,(%rcx)
    3df2:	00 00                	add    %al,(%rax)
    3df4:	00 00                	add    %al,(%rax)
	...

Disassembly of section .fini_array:

0000000000003df8 <__do_global_dtors_aux_fini_array_entry>:
    3df8:	e0 10                	loopne 3e0a <_DYNAMIC+0xa>
    3dfa:	00 00                	add    %al,(%rax)
    3dfc:	00 00                	add    %al,(%rax)
	...

Disassembly of section .dynamic:

0000000000003e00 <_DYNAMIC>:
    3e00:	01 00                	add    %eax,(%rax)
    3e02:	00 00                	add    %al,(%rax)
    3e04:	00 00                	add    %al,(%rax)
    3e06:	00 00                	add    %al,(%rax)
    3e08:	22 00                	and    (%rax),%al
    3e0a:	00 00                	add    %al,(%rax)
    3e0c:	00 00                	add    %al,(%rax)
    3e0e:	00 00                	add    %al,(%rax)
    3e10:	0c 00                	or     $0x0,%al
    3e12:	00 00                	add    %al,(%rax)
    3e14:	00 00                	add    %al,(%rax)
    3e16:	00 00                	add    %al,(%rax)
    3e18:	00 10                	add    %dl,(%rax)
    3e1a:	00 00                	add    %al,(%rax)
    3e1c:	00 00                	add    %al,(%rax)
    3e1e:	00 00                	add    %al,(%rax)
    3e20:	0d 00 00 00 00       	or     $0x0,%eax
    3e25:	00 00                	add    %al,(%rax)
    3e27:	00 f4                	add    %dh,%ah
    3e29:	11 00                	adc    %eax,(%rax)
    3e2b:	00 00                	add    %al,(%rax)
    3e2d:	00 00                	add    %al,(%rax)
    3e2f:	00 19                	add    %bl,(%rcx)
    3e31:	00 00                	add    %al,(%rax)
    3e33:	00 00                	add    %al,(%rax)
    3e35:	00 00                	add    %al,(%rax)
    3e37:	00 f0                	add    %dh,%al
    3e39:	3d 00 00 00 00       	cmp    $0x0,%eax
    3e3e:	00 00                	add    %al,(%rax)
    3e40:	1b 00                	sbb    (%rax),%eax
    3e42:	00 00                	add    %al,(%rax)
    3e44:	00 00                	add    %al,(%rax)
    3e46:	00 00                	add    %al,(%rax)
    3e48:	08 00                	or     %al,(%rax)
    3e4a:	00 00                	add    %al,(%rax)
    3e4c:	00 00                	add    %al,(%rax)
    3e4e:	00 00                	add    %al,(%rax)
    3e50:	1a 00                	sbb    (%rax),%al
    3e52:	00 00                	add    %al,(%rax)
    3e54:	00 00                	add    %al,(%rax)
    3e56:	00 00                	add    %al,(%rax)
    3e58:	f8                   	clc    
    3e59:	3d 00 00 00 00       	cmp    $0x0,%eax
    3e5e:	00 00                	add    %al,(%rax)
    3e60:	1c 00                	sbb    $0x0,%al
    3e62:	00 00                	add    %al,(%rax)
    3e64:	00 00                	add    %al,(%rax)
    3e66:	00 00                	add    %al,(%rax)
    3e68:	08 00                	or     %al,(%rax)
    3e6a:	00 00                	add    %al,(%rax)
    3e6c:	00 00                	add    %al,(%rax)
    3e6e:	00 00                	add    %al,(%rax)
    3e70:	f5                   	cmc    
    3e71:	fe                   	(bad)  
    3e72:	ff 6f 00             	ljmp   *0x0(%rdi)
    3e75:	00 00                	add    %al,(%rax)
    3e77:	00 b0 03 00 00 00    	add    %dh,0x3(%rax)
    3e7d:	00 00                	add    %al,(%rax)
    3e7f:	00 05 00 00 00 00    	add    %al,0x0(%rip)        # 3e85 <_DYNAMIC+0x85>
    3e85:	00 00                	add    %al,(%rax)
    3e87:	00 68 04             	add    %ch,0x4(%rax)
    3e8a:	00 00                	add    %al,(%rax)
    3e8c:	00 00                	add    %al,(%rax)
    3e8e:	00 00                	add    %al,(%rax)
    3e90:	06                   	(bad)  
    3e91:	00 00                	add    %al,(%rax)
    3e93:	00 00                	add    %al,(%rax)
    3e95:	00 00                	add    %al,(%rax)
    3e97:	00 d8                	add    %bl,%al
    3e99:	03 00                	add    (%rax),%eax
    3e9b:	00 00                	add    %al,(%rax)
    3e9d:	00 00                	add    %al,(%rax)
    3e9f:	00 0a                	add    %cl,(%rdx)
    3ea1:	00 00                	add    %al,(%rax)
    3ea3:	00 00                	add    %al,(%rax)
    3ea5:	00 00                	add    %al,(%rax)
    3ea7:	00 88 00 00 00 00    	add    %cl,0x0(%rax)
    3ead:	00 00                	add    %al,(%rax)
    3eaf:	00 0b                	add    %cl,(%rbx)
    3eb1:	00 00                	add    %al,(%rax)
    3eb3:	00 00                	add    %al,(%rax)
    3eb5:	00 00                	add    %al,(%rax)
    3eb7:	00 18                	add    %bl,(%rax)
    3eb9:	00 00                	add    %al,(%rax)
    3ebb:	00 00                	add    %al,(%rax)
    3ebd:	00 00                	add    %al,(%rax)
    3ebf:	00 15 00 00 00 00    	add    %dl,0x0(%rip)        # 3ec5 <_DYNAMIC+0xc5>
	...
    3ecd:	00 00                	add    %al,(%rax)
    3ecf:	00 03                	add    %al,(%rbx)
    3ed1:	00 00                	add    %al,(%rax)
    3ed3:	00 00                	add    %al,(%rax)
    3ed5:	00 00                	add    %al,(%rax)
    3ed7:	00 c0                	add    %al,%al
    3ed9:	3f                   	(bad)  
    3eda:	00 00                	add    %al,(%rax)
    3edc:	00 00                	add    %al,(%rax)
    3ede:	00 00                	add    %al,(%rax)
    3ee0:	07                   	(bad)  
    3ee1:	00 00                	add    %al,(%rax)
    3ee3:	00 00                	add    %al,(%rax)
    3ee5:	00 00                	add    %al,(%rax)
    3ee7:	00 30                	add    %dh,(%rax)
    3ee9:	05 00 00 00 00       	add    $0x0,%eax
    3eee:	00 00                	add    %al,(%rax)
    3ef0:	08 00                	or     %al,(%rax)
    3ef2:	00 00                	add    %al,(%rax)
    3ef4:	00 00                	add    %al,(%rax)
    3ef6:	00 00                	add    %al,(%rax)
    3ef8:	c0 00 00             	rolb   $0x0,(%rax)
    3efb:	00 00                	add    %al,(%rax)
    3efd:	00 00                	add    %al,(%rax)
    3eff:	00 09                	add    %cl,(%rcx)
    3f01:	00 00                	add    %al,(%rax)
    3f03:	00 00                	add    %al,(%rax)
    3f05:	00 00                	add    %al,(%rax)
    3f07:	00 18                	add    %bl,(%rax)
    3f09:	00 00                	add    %al,(%rax)
    3f0b:	00 00                	add    %al,(%rax)
    3f0d:	00 00                	add    %al,(%rax)
    3f0f:	00 1e                	add    %bl,(%rsi)
    3f11:	00 00                	add    %al,(%rax)
    3f13:	00 00                	add    %al,(%rax)
    3f15:	00 00                	add    %al,(%rax)
    3f17:	00 08                	add    %cl,(%rax)
    3f19:	00 00                	add    %al,(%rax)
    3f1b:	00 00                	add    %al,(%rax)
    3f1d:	00 00                	add    %al,(%rax)
    3f1f:	00 fb                	add    %bh,%bl
    3f21:	ff                   	(bad)  
    3f22:	ff 6f 00             	ljmp   *0x0(%rdi)
    3f25:	00 00                	add    %al,(%rax)
    3f27:	00 01                	add    %al,(%rcx)
    3f29:	00 00                	add    %al,(%rax)
    3f2b:	08 00                	or     %al,(%rax)
    3f2d:	00 00                	add    %al,(%rax)
    3f2f:	00 fe                	add    %bh,%dh
    3f31:	ff                   	(bad)  
    3f32:	ff 6f 00             	ljmp   *0x0(%rdi)
    3f35:	00 00                	add    %al,(%rax)
    3f37:	00 00                	add    %al,(%rax)
    3f39:	05 00 00 00 00       	add    $0x0,%eax
    3f3e:	00 00                	add    %al,(%rax)
    3f40:	ff                   	(bad)  
    3f41:	ff                   	(bad)  
    3f42:	ff 6f 00             	ljmp   *0x0(%rdi)
    3f45:	00 00                	add    %al,(%rax)
    3f47:	00 01                	add    %al,(%rcx)
    3f49:	00 00                	add    %al,(%rax)
    3f4b:	00 00                	add    %al,(%rax)
    3f4d:	00 00                	add    %al,(%rax)
    3f4f:	00 f0                	add    %dh,%al
    3f51:	ff                   	(bad)  
    3f52:	ff 6f 00             	ljmp   *0x0(%rdi)
    3f55:	00 00                	add    %al,(%rax)
    3f57:	00 f0                	add    %dh,%al
    3f59:	04 00                	add    $0x0,%al
    3f5b:	00 00                	add    %al,(%rax)
    3f5d:	00 00                	add    %al,(%rax)
    3f5f:	00 f9                	add    %bh,%cl
    3f61:	ff                   	(bad)  
    3f62:	ff 6f 00             	ljmp   *0x0(%rdi)
    3f65:	00 00                	add    %al,(%rax)
    3f67:	00 03                	add    %al,(%rbx)
	...

Disassembly of section .got:

0000000000003fc0 <_GLOBAL_OFFSET_TABLE_>:
    3fc0:	00 3e                	add    %bh,(%rsi)
	...

Disassembly of section .data:

0000000000004000 <__data_start>:
	...

0000000000004008 <__dso_handle>:
    4008:	08 40 00             	or     %al,0x0(%rax)
    400b:	00 00                	add    %al,(%rax)
    400d:	00 00                	add    %al,(%rax)
	...

Disassembly of section .bss:

0000000000004010 <completed.0>:
    4010:	00 00                	add    %al,(%rax)
	...

0000000000004014 <nCompletionStatus>:
    4014:	00 00                	add    %al,(%rax)
	...

Disassembly of section .comment:

0000000000000000 <.comment>:
   0:	47                   	rex.RXB
   1:	43                   	rex.XB
   2:	43 3a 20             	rex.XB cmp (%r8),%spl
   5:	28 55 62             	sub    %dl,0x62(%rbp)
   8:	75 6e                	jne    78 <__abi_tag-0x314>
   a:	74 75                	je     81 <__abi_tag-0x30b>
   c:	20 31                	and    %dh,(%rcx)
   e:	31 2e                	xor    %ebp,(%rsi)
  10:	34 2e                	xor    $0x2e,%al
  12:	30 2d 31 75 62 75    	xor    %ch,0x75627531(%rip)        # 75627549 <_end+0x75623531>
  18:	6e                   	outsb  %ds:(%rsi),(%dx)
  19:	74 75                	je     90 <__abi_tag-0x2fc>
  1b:	31 7e 32             	xor    %edi,0x32(%rsi)
  1e:	32 2e                	xor    (%rsi),%ch
  20:	30 34 29             	xor    %dh,(%rcx,%rbp,1)
  23:	20 31                	and    %dh,(%rcx)
  25:	31 2e                	xor    %ebp,(%rsi)
  27:	34 2e                	xor    $0x2e,%al
  29:	30 00                	xor    %al,(%rax)
