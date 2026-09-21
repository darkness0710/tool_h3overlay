# scripts/

Công cụ khảo sát bộ nhớ game cho h3overlay.

## Vì sao overlay này khác h3roviewer

Mọi offset h3overlay dùng đều neo vào **exe** (`h3hota.exe` / `h3hota HD.exe`):

| Offset | Trỏ vào | Dùng cho |
|---|---|---|
| `0x2992B8` | exe | status struct + player section |
| `0x299538` | exe | map info |
| `0x29d800` | exe | chat (trade) |
| `0xF1728` | exe | popup WIN/LOSE |
| `0x2AA694`, `0x2AAA20` | exe | Thieves' Guild: guild đang mở, best hero mỗi người |

HotA **không build lại exe** — file vẫn là bản 2023 — nên các offset này đứng
yên qua từng bản update. Đây là lý do h3overlay không cần script dò offset như
`h3roviewer/scripts/find_hota_offsets.py`: nó không có offset nào neo vào
`hota.dll`, thứ duy nhất bị dịch mỗi lần HotA build lại.

**Giữ nguyên tính chất này.** Trước khi thêm bất kỳ offset `hota.dll` nào, cân
nhắc rằng nó biến overlay thành thứ phải bảo trì lại sau mỗi bản HotA.

## probe_tavern_stats.py

Dò xem game có ghi sẵn **chỉ số sơ cấp mà Thieves' Guild đã tiết lộ** ở vùng
nhớ gần mảng best-hero (`exe + 0x2AAA20`) hay không.

Câu hỏi này quyết định việc có thêm được tính năng "hiện 4 chỉ số
công/thủ/sức mạnh/kiến thức của hero đối thủ" hay không:

- **Nếu có** — nguồn dữ liệu nằm trong exe (không vỡ theo update) và chỉ chứa
  đúng những gì người chơi được phép thấy. Thêm tính năng được.
- **Nếu không** — đường duy nhất còn lại là đọc `BaseHeroStruct` qua
  `hota.dll`, vốn trả về chỉ số **thật** bất kể guild đã tiết lộ hay chưa.
  Cách đó vừa làm overlay vỡ mỗi bản update, vừa có nguy cơ hiện thông tin
  người chơi chưa được phép biết.

### Chạy

Cần Python 3.6+ (chỉ dùng stdlib) và **game đang chạy**. Chạy bằng quyền
administrator nếu không attach được.

```bash
# 1. chụp khi CHƯA mở Thieves' Guild
python scripts/probe_tavern_stats.py --save closed.bin

# 2. mở Thieves' Guild trong game (ở trận có đối thủ đã lộ best hero), rồi:
python scripts/probe_tavern_stats.py --save open.bin --diff closed.bin
```

Output gồm: trạng thái con trỏ guild, mảng best hero từng người chơi, hexdump
vùng xung quanh với dấu `*` ở byte đã đổi giữa hai lần chụp, và danh sách các
cụm 4 số nhỏ liên tiếp — tức ứng viên cho công/thủ/sức mạnh/kiến thức.

Ứng viên đáng tin là cụm vừa **đổi** khi mở guild, vừa khớp với chỉ số hiện
trên màn hình game.

Exit code: `0` = đọc được, `2` = game không chạy hoặc không attach được.
