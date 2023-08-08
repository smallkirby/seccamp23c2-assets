# SecurityCamp2023 Assets

This repository provides the assets used in
[SecurityCamp2023](https://www.ipa.go.jp/jinzai/security-camp/2023/zenkoku/program_list_cd.html#:~:text=C2%E3%80%8E%E6%89%8B%E3%82%92%E5%8B%95%E3%81%8B%E3%81%97%E3%81%A6%E7%90%86%E8%A7%A3%E3%81%99%E3%82%8B%20Linux%20Kernel%20Exploit%E3%80%8F)
"C2 手を動かして理解するLinux Kernel Exploit" lecture.

## Contents

- `/src`: Challenges for the lecture. Each challenge has `UNIMPLEMENTED` code
that you need to implement.
- `/answer`: Answers for the challenges. You can use this as a reference.
- `/nirugiri`: Utility script for kernel exploit.

## Distributed Files

Distribution file is provided
You can get distributed assets at [GitHub Release page](https://github.com/smallkirby/seccamp23c2-assets/releases/tag/release).

- `dist.tar.gz`: Set of challenges and answers.
- `vmlinux.tar.gz`: `vmlinux` with debug symbols of kernel v5.14 with io_uring bug.
- `seccamp23c2-slide.pdf`: Slide for the lecture.
  - Written in Japanese.
  - Note that this slide simplifies some description.

## Note

The final exploit strategy has low reliability.
To stabilize the exploit, refer to the lecture slide.

## Thanks

This lecture was made possible by the contribution of [TSG](https://tsg.ne.jp/) members.
