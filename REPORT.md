# 🧪 과제 — UAF 실습 키트 (직접 컴파일하고 눈으로 본다)

> 멘티 이름 ______11반 김태형______  
>
> 이 과제는 **작은 C 예제 6개 + 챌린지 1개**를 직접 컴파일·실행하며 use-after-free 를
> "눈으로" 익히는 것이다. 각 예제는 실행하면 힙 상태가 **색깔로 그려진다**.
> 강의의 njs sort UAF 와 1:1로 연결된다.

---

## 0. 준비 — 어떻게 돌리나 (컴파일러 없어도 됨)

이 키트는 **도커 이미지 하나면** 돌아간다(njs 랩에서 빌드한 `whs/njs-sort-uaf:vuln`,
clang 내장). 로컬에 clang/gcc 가 있으면 그걸로도 된다.

```powershell
# Windows
.\run.ps1
```
```bash
# macOS / Linux / WSL / Git Bash
bash run.sh
```

한 번 돌리면 예제마다 **두 얼굴**이 출력된다 (njs 랩의 asan-dm vs dbg 두 얼굴과 같은 원리):
- **얼굴 A (no-ASan)** — 프로그램이 그냥 진행됨. 값이 어떻게 새거나 바뀌는지 **눈으로**.
- **얼굴 B (ASan)** — AddressSanitizer 가 바로 그 UAF 줄에서 **잡아 세운다**(리포트).

> 개별로 돌리고 싶으면(로컬 컴파일러 있을 때):
> `make 01_hello_uaf.n` (동작) · `make 01_hello_uaf.a` (ASan)

---

## 1. 관찰 기록 — 예제 6개 (각 예제 '두 얼굴' 다 보고 채우기)

각 예제를 돌리고 아래 표를 채워라. **색깔 라벨**(live/FREED/PLANTED/DANGLING)을 근거로.

| # | 파일 | 얼굴 A에서 '무엇이' 이상해졌나 (한 줄) | 얼굴 B(ASan)가 지목한 사고 종류 |
|---|---|---|---|
| 01 | `01_hello_uaf.c` | free로 해제한 메모리를 다시 읽으니 메모리 값이 변함 | heap-use-after-free |
| 02 | `02_realloc_move.c` | realloc으로 해제된 이전 버퍼를 cahed 포인터로 다시 읽었더니 메모리 값이 변함 | heap-use-after-free |
| 03 | `03_reentrancy.c` | 콜백을 만나 realloc되었는데 루프는 이전 주소를 계속해서 읽음 | heap-use-after-free | 
| 04 | `04_dos_vs_leak.c` | free된 메모리를 다시 읽으니, 재점유 여부에 따라 결과가 달라짐(재점유 X -> Dos, 재점유 O -> leak) | (A가 핵심) |
| 05 | `05_type_confusion.c` | 해제된 함수 포인터 객체의 메모리가 공격자에 의해 재점유되어, 댕글링 포인터로 호출 시 공격자가 지정한 함수가 실행됨| (A가 핵심) |
| 06 | `06_fixed.c` | (바뀐 값이 없어야 정상) | (조용하면 성공) |

---

## 2. 예측 → 확인 (실행 전에 먼저 적어라)

**Q2.1** (예제 02) `cached` 와 `p` 의 주소는 realloc 후 같을까 다를까? 왜?
> 이유: `cached`와 `p`의 주소는 같을 수도 있고 다를 수도 있다. realloc 함수는 기존 위치에서 메모리를 확장할 수 있으면 같은 주소를 유지하지만, 공간이 부족하면 새 위치로 이동할 경우에는 `p`는 새로운 주소로 변경되고, `cached`는 옛 주소를 그대로 가지기 때문이다.

**Q2.2** (예제 04) 케이스 A(재점유 없음)와 케이스 B(재점유)의 '읽힌 값'이 왜 다른가?
한 문장으로. 어느 쪽이 DoS 이고 어느 쪽이 leak 인가?
> 케이스 A의 경우 해제된 메모리가 재점유 되지 않아 제어 불가능한 값이 읽혀 DoS이고, 케이스 B의 경우 같은 자리를 새로운 데이터로 재점유하여 옛 포인터로 참조 시 그 값이 읽힐 수 있어 leak이다.

**Q2.3** (예제 05) UAF 로 읽은 것이 '함수 포인터'였다. 그래서 read 가 왜 곧 '무엇을
실행할지(제어)'가 되는가?
> 함수 포인터의 read는 호출 대상 함수의 주소를 읽는 것이기 때문에, 해제된 객체가 재점유되어 함수 포인터에 다른 함수의 주소가 저장된 상태에서 호출하면 변경된 주소의 함수가 실행된다.

**Q2.4** (예제 06) `free(p); p = NULL;` 은 UAF 를 '안전한 실패'로 바꾼다. 무슨 뜻인가?
그리고 njs 수정이 택한 (B) '캐시 대신 매번 재조회' 는 왜 더 근본적인가?
> free(p) 이후 포인터를 NULL로 초기화함으로 해제된 메모리를 다시 참조할 수 없게 되며, 실수로 참조하더라도 즉시 눈에 띄는 NULL 참조 오류가 발생하여 안전한 실패가 된다.<br>
> (B) '캐시 대신 매번 재조회'는 realloc 함수로 메모리 주소가 변경되더라도 매번 최신 포인터를 다시 조회하므로 옛 버퍼를 참조하지 않기 때문에 댕글링포인터가 생성되지 않아 UAF를 근본적으로 제거한다.

**Q2.5** (연결) 06 을 뺀 01~05 중, 강의의 **njs sort 버그와 가장 똑같은** 예제는? 왜?
> 예제 03  이유: 강의의 njs sort 버그 코드와 예제 03 코드 모두 루프 전에 캐시한 버퍼 포인터를, 루프 중 콜백에 의해 `realloc()` 또는 `njs_array_expand()`로 버퍼가 확장되어 새 버퍼를 할당하고 기존 버퍼를 `free()`한다. 그런데도 루프는 갱신된 버퍼를 사용하지 않고 캐시한 버퍼 포인터를 계속 사용하므로, 해제된 이전 버퍼를 참조하는 댕글링 포인터가 되어 UAF가 발생하는 동일한 구조를 가지기 때문이다.

---

## 3. 핵심 실습 — 버그를 직접 고쳐라 (`challenge_fixme.c`)

`challenge_fixme.c` 에는 UAF 가 **하나** 숨어 있다.

**할 일**: 프로그램의 의도(마지막에 첫 원소 값 출력)는 그대로 두고, UAF 만 없애라.
**성공 기준**: ASan 으로 돌렸을 때 `heap-use-after-free` 가 **안 뜬다**.

```bash
# 고치기 전: ASan 이 잡는지 먼저 확인
clang -fsanitize=address -g challenge_fixme.c -o /tmp/c && /tmp/c    # (도커 셸 안 또는 로컬)
# 고친 뒤: 조용하면 성공
```

**Q3.1** 어느 포인터가 realloc 을 건너서 재사용됐나? (변수명)
> first

**Q3.2** 어떻게 고쳤나? (붙여넣기 — 바꾼 줄만)
```c
printf("     first 첫 원소 읽기 => %d\n", v.a[0]);
```
**Q3.3** 왜 그 수정이 UAF 를 없애는지 한 문장으로 서술.
> 캐시된 옛 포인터를 사용하지 않고, `realloc()`이후 갱신된 현재 버퍼 `v.a`를 다시 인덱싱해 읽으므로 해제된 메모리를 참조하지 않아 UAF가 발생하지 않는다.

---

## 4. 보너스 — 나만의 UAF 만들기 (선택, 가산점)

`my_uaf.c` 를 새로 하나 작성하라. 조건:
- `viz.h` 를 써서 힙 상태를 **색으로** 그릴 것(최소 `viz_cell` 3번).
- ASan 으로 컴파일하면 `heap-use-after-free` 가 뜰 것.
- 01~05 와 **다른** 방식이면 더 좋다.

```bash
clang -fsanitize=address -g my_uaf.c -o /tmp/my && /tmp/my
```

**Q4.1** 당신의 UAF 의 '한 줄 요약'(무엇을 free 하고 어디서 다시 쓰나):
> ______________________________________________________________

<br>

# 📄 과제 2

> 📄 uaf-ctf의 exploit.py 코드를 먼저 분석한다. player 환경에서 pwngdb를 이용하여 exploit.py에서 확인된 부분들을 역분석 한다.

---
## 🔴 exploit.py 코드 분석

> 🔑 owner/exploit.py 를 보고 익스플로잇 원리를 분석한다.

### 1️⃣ win 함수 주소 찾기

- `win_addr_from_elf` 함수를 보면, `nm/readelf` 시스템 도구를 이용해 `win` 심볼을 찾고 있음
    
    ![img](/images/1.png)
    
- 위 과정에서 `win` 심볼을 찾지 못할경우, 파이썬 ELF64 파일 포맷을 직접 파싱해 `win` 심볼 주소를 찾음
    
    ![img](/images/2.png)
    

### 2️⃣ 통신 채널 생성

- class Tube - 소켓 기반(원격) - nc로 열려있는 취약 프로그램과의 통신
    
    ![img](/images/3.png)
    
- class ProcTube - subprocess 기반(로컬) - 로컬 바이너리 사이의 파이프(stdin/stdout) 통신
    
    ![img](/images/4.png)
    
- `Tube`와 `ProcTube`는 취약한 프로그램과 익스플로잇 스크립트(Host PC) 사이에서, 데이터를 주고받을 수 있는 동일한 인터페이스(`until, send, drain`)를 제공하는 통신 래퍼이다. 실제 익스플로잇이 이용하는 통로

### 3️⃣ 익스플로잇

- UAF 익스플로잇을 수행
    
    ![img](/images/5.png)
    
    - 익스플로잇 흐름
        - create → 객체 생성, msg>pwn
        - free → 생성된 객체를 free, but 포인터는 유지 → 댕글링 포인터
        - stash → free 된 메모리 영역에 8바이트 만큼 win 함수의 주소를 넣음
        - greet → 객체의 함수 포인터를 호출(win 함수)

---
## 🟠 실습 과제 + 역분석 진행

> 🔑 player 환경에서 pwngdb를 이용하여 UAF가 발생하는 과정을 분석한다.

### 1️⃣ `checksec` 로 이 바이너리에서 PIE가 꺼져 있음을 확인하고, 그게 왜 익스를 쉽게 하는지 한 줄로 설명하라

- `pwngdb> checksec`
    
    ![img](/images/6.png)
    
    - No PIE는 실행 파일이 항상 동일한 가상 주소에 로드되는 것으로, `win()` 주소가 변하지 않기 때문에 ASLR 우회가 불필요함. 미리 알아낸 함수 주소를 그대로 사용하여 익스플로잇을 쉽게 수행할 수 있다.

### 2️⃣ create → `x/4gx obj` 로 greet 함수 포인터의 초기값(default_greet)을 적어라

- 객체 생성
    
    ![img](/images/7.png)
    
- `pwndbg> x/4gx obj`
    
    ![img](/images/8.png)
    
    - `obj→greet` 함수 포인터의 초기 값은 default_greet(0x400aa8) 이다.
        
        ![img](/images/9.png)
        

### 3️⃣ free 직후 `p obj` 가 NULL 이 아님을 캡처하라

- 객체 반납
    
    ![img](/images/10.png)
    
- `pwndbg> p obj`
    
    ![img](/images/11.png)
    
    - obj 포인터가 `free(obj)` 함수 이후에도 초기화되지 않고 0x3e03f2a0 주소를 저장(옛 주소를 저장, 즉 댕글링 포인터)하고 있다.
    - 이후 obj 포인터를 역참조하면 UAF 발생할 수 있다.

### 4️⃣ free 후 `bins` 에서 어떤 bin(크기)으로 들어갔는지 적어라.

- `pwndbg> bins`
    
    ![img](/images/12.png)
    
    - free를 통해 해제된 obj 청크가 tcache의 0x30 크기 bin으로 들어갔다.
        - obj는 greet 8byte, msg 24byte로 32byte(0x20)이나, 청크 관리 정보가 붙어서 0x30 청크
        - glibc malloc은 사용자 영역 외에도 청크 헤더를 포함한다.

### 5️⃣ stash 후 `p obj`, `p raw` 가 같은 주소임을 확인하라.

- `pwndbg> p obj`
    
    ![img](/images/13.png)
    

- `p raw`
    
    ![img](/images/14.png)
    
- `malloc(32)`가 free된 tcache 청크를 반환하며 raw 포인터와 obj 포인터가 동일한 주소를 가리키게 되었다.
- raw 포인터가 obj 포인터의 옛 주소를 재점유한 것을 확인할 수 있다.
    - 주소 동일 0x3e03f2a0

### 6️⃣ `x/gx obj` 로 greet 포인터가 win 주소로 덮인 것을 캡처하라.

- `set obj→greet = &win` 명령을 통해 obj→greet 함수 포인터에 win 주소 덮기
- `pwndbg> x/gx obj`
    
    ![img](/images/15.png)
    
    - `obj->greet` 함수 포인터가 win 주소(0x400ad8)로 덮인 것을 확인할 수 있다.
        
        ![img](/images/16.png)
        

### 7️⃣ `break win` → greet 호출 → 멈춘 뒤 `bt`로 greet 자리에서 win이 실행됨을 보여라

- `pwndbg> break win`
    
    ![img](/images/17.png)
    
    - win 함수에 브레이크 포인트 설정
- `pwndbg> continue`, 이후 greet 호출
- `pwndbg> bt`
    
    ![img](/images/18.png)
    
    - `bt` 결과를 통해 원래 `obj→greet()` 함수 포인터가 호출되는 위치에서 실제 실행 흐름이 `win()` 으로 변경되었음을 확인할 수 있다.
- `pwndbg> finish`
    
    ![img](/images/19.png)
    
    - `finish` 로 `win()` 실행을 마친 뒤 플래그가 출력되는 것을 확인할 수 있었다.