/* =============================================================================
 *  참조 카운팅(refcount) UAF
 * -----------------------------------------------------------------------------
 *  개념: 여러 곳에서 공유하는 자원의 해제를 refcount를 통해 판단할 경우,
 *       refcount가 0이 되는 "그 순간"에만 실제로 메모리가 해제된다.
 *       같은 unref() 호출이 어떤 때는 안전하고 어떤 때는 UAF를 유발한다.
 * ========================================================================== */
#include <stdio.h>
#include <stdlib.h>
#include "viz.h"

typedef struct {
    int refcount;
    int value;
} Buffer;

Buffer *buf_new(int value) { // 버퍼 할당
    Buffer *b = malloc(sizeof(Buffer));
    b->refcount = 1;
    b->value = value;
    return b;
}

void buf_ref(Buffer *b) { // 같은 버퍼 참조
    b->refcount++;
}

void buf_unref(Buffer *b) { // 참조 끝
    b->refcount--;
    if (b->refcount == 0) {
        free(b);   // 마지막 참조였다면 여기서 실제 해제 발생
    }
}

int main(void)
{
    viz_title("참조 카운팅(refcount) UAF", "같은 unref()호출이 어떤 때는 안전하고 어떤 때는 UAF를 유발");

    viz_step("Buffer 생성, refcount=1");
    Buffer *owner_a = buf_new(42);
    viz_cell("owner_a", owner_a, "refcount=1, value=42", VZ_LIVE);

    viz_step("다른 곳에서 같은 버퍼를 참조 (buf_ref)");
    Buffer *owner_b = owner_a;
    buf_ref(owner_b);   // refcount=2
    viz_cell("owner_b (같은 객체 공유)", owner_b, "refcount=2, value=42", VZ_LIVE);

    viz_step("owner_a 가 볼일이 끝나서 unref");
    buf_unref(owner_a);   // refcount=2 -> 1, 아직 안전
    viz_cell("owner_a", owner_a, "refcount=1 (아직 살아있음)", VZ_LIVE);

    viz_step("owner_b 도 볼일이 끝나서 unref -> refcount=0, free 발생!");
    buf_unref(owner_b);   // refcount=1 -> 0, 여기서 진짜 free() 실행됨
    viz_cell("owner_b", owner_b, "free()됨 - refcount==0", VZ_FREED);
    
    viz_note("owner_a 와 owner_b 는 여전히 같은 옛 주소를 들고 있다 (댕글링)");
    viz_cell("owner_a", owner_a, "옛 주소", VZ_DANGLE);
    viz_cell("owner_b", owner_b, "옛 주소", VZ_DANGLE);

    viz_step("반납한 owner_a를  다시 읽음 (UAF)");
    printf("\n");
    printf("owner_a->value = %d\n", owner_a->value);   // <-- heap-use-after-free

    viz_bad("owner_a 는 이미 refcount 0으로 해제된 메모리를 가리키고 있었다.");

    return 0;
}