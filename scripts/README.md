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

**Hạn chế:** chỉ dump ±1.5KB quanh mảng best-hero. Nếu chỉ số được lưu xa hơn
thì script này không thấy. Dùng `find_guild_stats.py` bên dưới thay thế.

## find_guild_stats.py

Quét **toàn bộ** memory của game để tìm đúng những con số đang hiện trên bảng
Thieves' Guild. Đọc chỉ số trên màn hình rồi truyền vào:

```bash
python scripts/find_guild_stats.py --stats 1,0,3,2 0,2,1,2 2,2,1,1
```

Thứ tự các `--stats` theo đúng thứ tự cột (1st, 2nd, 3rd).

Script tìm cả hai kiểu lưu (4 byte liền nhau, và 4 giá trị uint32), rồi gom
các kết quả nằm gần nhau thành cụm. **Cụm chứa nhiều hơn một người chơi mới
đáng quan tâm** — một cụm 4 số nhỏ đứng lẻ gần như luôn là trùng ngẫu nhiên,
còn nhiều người chơi nằm cạnh nhau chính là hình dạng của một bảng hiển thị.

Mỗi kết quả được quy về module chứa nó, và đó là điều quyết định:

| Nằm ở | Kết luận |
|---|---|
| `h3hota.exe+0x...` | Dùng được — exe không bị build lại từ 2023 |
| `hota.dll+0x...` | Dùng được nhưng vỡ mỗi bản HotA update |
| heap | Cần lần được chuỗi con trỏ từ một chỗ ổn định |

### Lưu ý: game có hai đường vào bảng này

Vào qua Tavern và vào thẳng Thieves' Guild có thể không chạy cùng một đoạn
code. Con trỏ `exe+0x2AA694` mà overlay đang dùng để biết "tavern đang mở"
chỉ được set ở một trong hai đường — nên nó bằng 0 **không** có nghĩa là bảng
đang đóng. Nếu quét một đường không ra kết quả, thử nốt đường kia.

Điều này cũng có nghĩa tính năng hiện tại (hiện best hero của đối thủ) có thể
đang bỏ sót khi người chơi vào bảng bằng đường không qua Tavern.

## probe_guild_gate.py

Trả lời một câu duy nhất: **game có tự duy trì mảng best-hero, hay chỉ ghi vào
lúc vẽ bảng Thieves' Guild?**

Câu này quyết định overlay có hiện được 4 chỉ số của đối thủ mà không bắt
người chơi mở bảng hay không:

- **Tự duy trì** — mảng đã đúng ngay từ đầu trận. Slot rỗng nghĩa là guild
  không cho xem, slot có hero nghĩa là có. Cổng hoàn chỉnh, không cần offset
  mới nào.
- **Chỉ ghi lúc vẽ** — overlay không phân biệt được "guild không cho xem" với
  "chưa mở bảng lần nào", vì cả hai đều rỗng. Phải đi tìm nơi lưu số lượng
  Thieves' Guild.

```bash
# vào trận, CHUA mo bang, rồi chạy:
python scripts/probe_guild_gate.py --watch
```

Nó in một dòng mỗi khi mảng hoặc con trỏ tavern đổi. Điều cần nhìn: mảng có
điền vào **trước** khi bảng được mở lần nào không — script đánh dấu rõ trường
hợp đó.

Chạy không tham số để đọc một lần. Nó cũng in player section (màu, số hero,
số town, danh sách hero sở hữu) để đối chiếu.

### Đã đo được: guild tiết lộ theo nhiều nấc

Đo trên HotA 1.8.1, ván mới Month 1 Week 1 Day 1, chưa có Thieves' Guild nào:

| Mức | Bảng hiện gì | Mảng `0x2AAA20` |
|---|---|---|
| 0 | Số town, số hero, **chân dung** best hero | **đã đầy đủ** |
| cao hơn | thêm Gold, Wood & Ore, **chỉ số** best hero | đầy đủ |

Nghĩa là mảng ứng với **chân dung**, không ứng với **chỉ số**. Nó đầy ngay từ
lượt 1 trong khi ô chỉ số trên màn hình vẫn trống.

**Hệ quả:** không được dùng mảng này làm cổng để quyết định có hiện 4 chỉ số
của đối thủ hay không. Làm vậy là hiện thông tin game chưa cho người chơi xem.
Cổng đúng phải là mức tiết lộ, tức số lượng Thieves' Guild sở hữu, và chỗ đó
vẫn chưa tìm ra.

### Chụp và so sánh dialog

Game quyết định vẽ hay không vẽ ô chỉ số ở đâu đó. Ứng viên là object dialog
mà `exe+0x2AA694` trỏ tới:

```bash
# mở bảng khi CHUA lo chi so (ván mới, chưa xây guild):
python scripts/probe_guild_gate.py --dialog-save dialog_nostats.bin

# xây Thieves' Guild cho tới khi ô chỉ số hiện ra, mở bảng lại:
python scripts/probe_guild_gate.py --dialog-diff dialog_nostats.bin
```

Dialog được dựng lại ở địa chỉ heap khác mỗi lần mở nên so sánh theo offset
trong object. Con trỏ bị dựng lại sẽ đổi lung tung và vô nghĩa, nên script
tách riêng những byte **nhỏ ở cả hai phía** lên đầu — một mức tiết lộ là số
nhỏ, con trỏ thì không.
