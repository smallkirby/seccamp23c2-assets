# SecurityCamp2023 Assets

This repository provides the assets used in
[SecurityCamp2023](https://www.ipa.go.jp/jinzai/security-camp/2023/zenkoku/program_list_cd.html#:~:text=C2%E3%80%8E%E6%89%8B%E3%82%92%E5%8B%95%E3%81%8B%E3%81%97%E3%81%A6%E7%90%86%E8%A7%A3%E3%81%99%E3%82%8B%20Linux%20Kernel%20Exploit%E3%80%8F)
`"C2 手を動かして理解するLinux Kernel Exploit"` lecture.

## Security Camp 2023

At Security Camp 2023 organized mainly by IPA, we learned the basics of userland/kernel exploit.
This repository hosts all the assets used during the lecture I had.
This lecture was centered on CVE-2021-41073 `io_uring` type confusion bug.
We built exploits step by step to extend our primitives starting from a simple UAF.

## Contents

- `/src`: Challenges for the lecture. Each challenge has `UNIMPLEMENTED` code
that you need to implement.
- `/answer`: Answers for the challenges. You can use this as a reference.
- `/nirugiri`: Utility script for kernel exploit.

## Distributed Files

You can get assets at [GitHub Release page](https://github.com/smallkirby/seccamp23c2-assets/releases/tag/release).

- `dist.tar.gz`: Set of challenges and answers.
- `vmlinux.tar.gz`: `vmlinux` with debug symbols of kernel v5.14 with io_uring bug.
- `seccamp23c2-slide.pdf`: Slide for the lecture.
  - Written in Japanese.
  - Note that this slide simplifies some description for the simplicity. If you find any mistakes, contact me.
 
We also used [P3LAND](https://p3land.smallkirby.com) as a prior learning.

## Note

The final exploit strategy has low reliability.
To stabilize the exploit, refer to the lecture slide.

## Contact

To improve, fix, or ask questions about the contents, contact me ([@smallkirby](https://twitter.com/smallkirby)) by sending DM, mentioning on Twitter, or filing a issue on GitHub.

## Community Writeups

List of writeups I found.
If you built your own exploit, please let me know :)

- [Exploit](https://github.com/robbert1978/kernel_for_fun/blob/main/CVE-2021-41073/exp_rop.c) by [@robbert1978](https://github.com/robbert1978): improve stability by ROP

## Thanks

This lecture was made possible by the contribution of [TSG](https://tsg.ne.jp/) members.
