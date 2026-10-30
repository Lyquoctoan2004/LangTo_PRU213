import json
import os
import sys

OUT = sys.argv[1]
os.makedirs(OUT, exist_ok=True)

CHIEF = "Bà Xuân · Trưởng làng"
SILK = "Ông Khánh · Nghệ nhân nuôi tằm"
DYER = "Cô Mai · Nghệ nhân se tơ & nhuộm"
WEAV = "Cô Lụa · Nghệ nhân dệt & may"

dialogue = []
rules = []


def line(speaker, text):
    return {"Speaker": speaker, "Text": text}


def d(name, lines, choices=None, actions=None):
    dialogue.append({"Name": name, "Lines": [line(s, t) for s, t in lines], "Choices": choices or [], "Actions": actions or []})


def ch(text, actions, nxt="None"):
    return {"Text": text, "Actions": actions, "NextDialogue": nxt}


def rule(name, npc, require, block, dialogue_name):
    rules.append({"Name": name, "NpcId": npc, "RequireFlags": require, "BlockFlags": block, "Dialogue": dialogue_name})


# ---------------------------------------------------------------- Chief: opening, side quest, menus
d("CHIEF_D6_OPEN", [
    (CHIEF, "(Sáng sớm. Bà đứng trước bảng thông báo của làng: NGÀY HỘI TƠ LỤA – CÒN 3 NGÀY.) Con đã học được rất nhiều."),
    (CHIEF, "Nhưng lễ hội năm nay sẽ không dễ dàng. Ba ngày tới, con không có nhiệm vụ bắt buộc nào cả."),
    (CHIEF, "Nếu muốn trở thành một người thợ giỏi hơn, hãy luyện nghề. Muốn có một bộ trang phục đẹp hơn, hãy làm. Muốn hiểu ngôi làng này hơn, hãy nói chuyện với mọi người."),
    (CHIEF, "Mỗi ngày con có bốn lượt hoạt động: luyện nghề ở nhà luyện tập, học hỏi các nghệ nhân, giúp họ một tay… Con không thể làm hết mọi thứ, nên hãy chọn cho khôn."),
    (CHIEF, "Ba ngày này thuộc về con. Khi muốn nghỉ hết ngày, cứ đến gặp bà."),
], actions=["Progress:Q30.TalkChief"])

d("CHIEF_D6_PATTERN", [
    (CHIEF, "Hoa văn đã thất truyền à? (Bà ngẫm nghĩ một lúc.) Có đấy. Hồi bà còn nhỏ, bà ngoại bà dệt một hoa văn mà giờ không ai nhớ."),
    (CHIEF, "Người ta bảo mảnh hoa văn cuối cùng nằm lại ở căn nhà cũ gần rìa rừng, phía sau xưởng. Con đi tìm xem – lấy được rồi mang về cho cô Lụa."),
], actions=["Progress:Q34.AskElder", "SetFlag:Gather.On", "AddCulture:3"])

d("CHIEF_D6_LESSON", [
    (CHIEF, "Hoa văn trên vải là chữ viết của làng mình, con ạ. Núi, hoa, nước suối – mỗi họa tiết ghi lại một điều tổ tiên muốn dặn con cháu."),
    (CHIEF, "Khi con mặc một bộ đồ có hoa văn truyền thống lên sân khấu, con không chỉ mặc đồ đẹp. Con đang kể một câu chuyện."),
])

for n in (1, 2, 3):
    learn_choice = ch("Hỏi bà về hoa văn và ý nghĩa của chúng (tốn 1 lượt).",
                      ["UseSlot:1", "AddCulture:6", "AddRel:NPC_Chief:3", "SetFlag:Lesson.NPC_Chief"], "CHIEF_D6_LESSON")
    if n < 3:
        rest = ch("Cháu xin nghỉ hết ngày hôm nay.", ["EndDay:D6_%d" % n])
        back = ch("Cháu quay lại chuẩn bị tiếp ạ.", [])
        intro = "(Ngày chuẩn bị thứ %d trong ba ngày.) Con muốn làm gì?" % n
    else:
        rest = ch("I'M READY. – Cháu đã sẵn sàng.", ["EndDay:D6_3"])
        back = ch("NOT YET. – Cháu vẫn cần thêm thời gian.", [])
        intro = "(Ngày cuối cùng.) Ngày mai lễ hội sẽ bắt đầu. Con đã sẵn sàng chưa?"
    d("CHIEF_D6_MENU%d_L" % n, [(CHIEF, intro)], [learn_choice, rest, back])
    d("CHIEF_D6_MENU%d" % n, [(CHIEF, intro)], [rest, back])

rule("CHIEF_D6_00", "NPC_Chief", ["Quest.Q30.Active"], ["Obj.Q30.TalkChief"], "CHIEF_D6_OPEN")
rule("CHIEF_D6_01", "NPC_Chief", ["Quest.Q34.Active"], ["Obj.Q34.AskElder"], "CHIEF_D6_PATTERN")
for n in (3, 2, 1):
    rule("CHIEF_D6_L%d" % n, "NPC_Chief", ["Free.Days", "Fday.%d" % n], ["Lesson.NPC_Chief", "Slots.Out"], "CHIEF_D6_MENU%d_L" % n)
    rule("CHIEF_D6_M%d" % n, "NPC_Chief", ["Free.Days", "Fday.%d" % n], [], "CHIEF_D6_MENU%d" % n)

# ---------------------------------------------------------------- Artisans: three lessons each, help, busy
d("HELP_GENERIC", [
    ("", "Bạn dành thời gian giúp một tay: xách nước, phân loại nguyên liệu, dọn dẹp xưởng. Người được giúp ghi nhớ điều đó."),
])
d("FREE_BUSY", [
    ("", "Hôm nay đã hết thời gian hoạt động (hoặc bạn đã học với người này rồi). Về gặp bà Xuân để nghỉ, ngày mai lại tiếp."),
])

NPCS = {
    "NPC_SilkMaster": {
        "tag": "SILK", "name": SILK, "skill": "Care", "extra1": ["StartQuest:Q33"],
        "lessons": [
            "Tằm không biết nói, nhưng chúng cho con biết hết: lúc đói, lúc nóng, lúc ẩm. Con chỉ cần nhìn kỹ là được.",
            "Kén chín dần từ một đầu. Hái non thì kén yếu, để già thì kén mất chất. Mắt phải nhìn, tay phải chờ – con học nhịp đó rồi.",
            "(Ông kéo con ra riêng.) Ta dạy con một mẹo: khi kéo tơ, đừng nhìn sợi – nhìn nồi nước. Nước hơi lăn tăn là sợi vừa. Đó là Kỹ thuật kéo tơ hoàn hảo của ta.",
        ],
        "tech": ["SetFlag:Bonus.Technique", "SetFlag:Technique.PerfectReeling", "Notify:Học được Kỹ thuật kéo tơ hoàn hảo – dải thời gian các mini-game rộng hơn một chút"],
        "help": "Giúp ông Khánh xếp những nong tằm và thay lá dâu mới",
    },
    "NPC_ThreadDyer": {
        "tag": "DYER", "name": DYER, "skill": "Dye", "extra1": [],
        "lessons": [
            "Cùng một lá chàm, nhúng sớm ra màu xanh nhạt, ngâm lâu thành xanh thẫm. Màu không nằm trong lá – nó nằm trong tay người nhuộm.",
            "Nước suối đầu nguồn nhuộm đẹp hơn nước giếng. Cô có một công thức nhuộm đỏ từ củ và vỏ cây, cô chỉ cho con.",
            "(Cô thì thầm.) Công thức này cô giấu bao năm: pha thêm chút tro bếp vào nước nhuộm, màu đỏ sẽ sâu và bền hơn. Đó là Công thức nhuộm hiếm – con giữ lấy.",
        ],
        "tech": ["SetFlag:Bonus.Technique", "SetFlag:Recipe.RareDye", "Notify:Học được Công thức nhuộm hiếm – dải thời gian các mini-game rộng hơn một chút"],
        "help": "Giúp cô Mai gom lá và vỏ cây cho nồi nhuộm của cô",
    },
    "NPC_WeaverTailor": {
        "tag": "WEAVER", "name": WEAV, "skill": "Weave", "extra1": ["StartQuest:Q34"],
        "lessons": [
            "Dệt là giữ nhịp. Chân đạp, tay đưa thoi, mắt nhìn sợi – ba việc cùng một nhịp. Khi con không còn nghĩ đến từng việc nữa là con đã biết dệt.",
            "May cũng vậy, nhưng chậm hơn và cẩn thận hơn. Một đường may lệch bằng một sợi tóc, đến lúc mặc lên người sẽ thấy rõ ngay.",
            "(Cô đặt tay lên vai con.) Cô dạy con Kỹ thuật dệt nâng cao: đổi tay thoi giữa chừng, vải sẽ đều hơn rất nhiều. Cô học nó từ mẹ cô.",
        ],
        "tech": ["SetFlag:Bonus.Technique", "SetFlag:Technique.AdvancedWeaving", "Notify:Học được Kỹ thuật dệt nâng cao – dải thời gian các mini-game rộng hơn một chút"],
        "help": "Giúp cô Lụa cuộn vải và kiểm tra các khung dệt cho kịp lễ hội",
    },
}

for npc, info in NPCS.items():
    t, nm = info["tag"], info["name"]
    for lvl in (1, 2, 3):
        learn_actions = ["UseSlot:1", "AddCulture:%d" % (3 + lvl), "AddRel:%s:3" % npc, "SetFlag:Lesson.%s" % npc,
                         "SetFlag:Learn%d.%s" % (lvl, npc), "AddSkill:%s:20" % info["skill"]]
        if lvl == 1:
            learn_actions += info["extra1"]
        if lvl == 3:
            learn_actions += info["tech"]
        help_actions = ["UseSlot:1", "AddRel:%s:5" % npc, "SetFlag:Lesson.%s" % npc, "AddSkill:%s:25" % info["skill"]]
        d("%s_MENU%d" % (t, lvl), [(nm, "(Ông bà, cô chú nhìn con và mỉm cười.) Con muốn làm gì hôm nay?" if False else "Con đến đúng lúc đấy. Hôm nay con muốn học hay muốn giúp một tay?")], [
            ch("Học hỏi nghề (tốn 1 lượt).", learn_actions, "%s_LESSON%d" % (t, lvl)),
            ch(info["help"] + " (tốn 1 lượt).", help_actions, "HELP_GENERIC"),
            ch("Dạ, để lúc khác ạ.", []),
        ])
        d("%s_LESSON%d" % (t, lvl), [(nm, info["lessons"][lvl - 1])])
        previous = ["Learn%d.%s" % (lvl - 1, npc)] if lvl > 1 else []
        rule("%s_D6_L%d" % (t, lvl), npc, ["Free.Days"] + previous, ["Lesson.%s" % npc, "Slots.Out", "Learn%d.%s" % (lvl, npc)], "%s_MENU%d" % (t, lvl))
    rule("%s_D6_BUSY" % t, npc, ["Free.Days"], [], "FREE_BUSY")

# Lost Pattern: bring the fragment back to Lụa.
d("WEAVER_D6_PATTERN_DONE", [
    (WEAV, "(Cô áp mảnh vải cũ lên bàn, mắt sáng lên.) Đây rồi… hoa văn của bà ngoại bà Xuân! Cô tưởng không còn ai nhớ nó nữa."),
    (WEAV, "Con tìm lại được nó cho làng. Cô sẽ cho con dùng hoa văn cổ này cho trang phục của con – chỉ cần nói với cô."),
], actions=["Progress:Q34.ReturnTailor"])
rule("WEAVER_D6_PATTERN", "NPC_WeaverTailor", ["Quest.Q34.Active", "Obj.Q34.FindFragment"], ["Obj.Q34.ReturnTailor"], "WEAVER_D6_PATTERN_DONE")
d("WEAVER_D6_ANCIENT", [
    (WEAV, "Con muốn dùng hoa văn cổ cho trang phục chứ? (Cô lấy mảnh vải mẫu ra.) Cô sẽ thêu lại lên bộ đồ cho con."),
], [
    ch("Dạ, cháu muốn dùng hoa văn cổ.", ["ClearFlags:Out.Pat.", "SetFlag:Out.Pat.Ancient", "SetFlag:Ancient.Applied", "Notify:Trang phục đã có Hoa văn cổ (Ancient Pattern)"]),
    ch("Để cháu nghĩ thêm.", []),
])
# Applying the ancient pattern comes up once the day's lesson is used (before the "busy" fallback).
busy_at = next(i for i, r in enumerate(rules) if r["Name"] == "WEAVER_D6_BUSY" or r["Name"] == "WEAVER_D6_BUSY".replace("WEAVER", "WEAVER"))
rules.insert(busy_at, {"Name": "WEAVER_D6_ANC", "NpcId": "NPC_WeaverTailor", "RequireFlags": ["Free.Days", "Unlock.AncientPattern"],
                       "BlockFlags": ["Ancient.Applied"], "Dialogue": "WEAVER_D6_ANCIENT"})
# the Lost Pattern hand-in comes before the lesson menus; the festival chief rules go first of all
rules.sort(key=lambda r: 0 if r["Name"] == "WEAVER_D6_PATTERN" else 1)

d("CHIEF_F_PHASE_SELECT", [(CHIEF, "Thử thách đã bắt đầu. Trước tiên, chọn nguyên liệu đi con.")], actions=["Dialogue:FEST_SELECT"])
festival_rules = [
    ("CHIEF_F_00", ["Quest.Q43.Active"], ["Obj.Q43.Select"], "CHIEF_F_PHASE_SELECT"),
    ("CHIEF_F_01", ["Quest.Q43.Active"], [], "CHIEF_F_PHASE_HINT"),
    ("CHIEF_F_02", ["Quest.Q42.Active"], ["Obj.Q42.TalkChief"], "CHIEF_F_CHALLENGE"),
    ("CHIEF_F_03", ["Quest.Q41.Active"], [], "CHIEF_F_HINT"),
    ("CHIEF_F_04", ["Quest.Q40.Active"], ["Obj.Q40.TalkChief"], "CHIEF_F_OPEN"),
]
for i, (nm_, req, blk, dlg) in enumerate(festival_rules):
    rules.insert(i, {"Name": nm_, "NpcId": "NPC_Chief", "RequireFlags": req, "BlockFlags": blk, "Dialogue": dlg})

# ---------------------------------------------------------------- Festival
d("CHIEF_F_OPEN", [
    (CHIEF, "(Cổng làng phủ đầy vải, các gian hàng đã mở, du khách bắt đầu đến, nhạc vang lên. Bà đứng giữa quảng trường.) Chào mừng! Ngày hội Tơ Lụa chính thức bắt đầu!"),
    (CHIEF, "Con cứ đi quanh làng mà xem: chợ lụa, khu nghệ nhân trình diễn nghề, khu văn hóa. Khi nào xem đủ, quay lại gặp bà."),
], actions=["Progress:Q40.TalkChief", "StartQuest:Q41"])
d("CHIEF_F_HINT", [
    (CHIEF, "Con đi xem chợ lụa, khu nghệ nhân và khu văn hóa đi. Bà cần con biết cả ngôi làng trước khi bước lên sân khấu.")])
d("CHIEF_F_CHALLENGE", [
    (CHIEF, "(Lễ hội đã gần kết thúc. Bà gọi con ra riêng.) Con đã đi một chặng đường dài."),
    (CHIEF, "Nhưng nếu muốn thực sự trở thành một người học nghề của ngôi làng, con phải hoàn thành thử thách cuối cùng: THỬ THÁCH TƠ LỤA – THE MASTER'S CHALLENGE."),
    (CHIEF, "Đây không phải một mini-game: là cả một chuỗi – chọn nguyên liệu, nhuộm đúng màu lễ hội, dệt, may rồi mặc bộ đồ bước lên sân khấu. Những gì con chuẩn bị ba ngày qua sẽ quyết định."),
], [
    ch("Cháu đã sẵn sàng.", ["Progress:Q42.TalkChief", "Dialogue:FEST_SELECT"]),
    ch("Cháu chưa sẵn sàng, cho cháu đi dạo thêm.", []),
])
d("FEST_SELECT", [
    (CHIEF, "PHASE 1 – CHỌN NGUYÊN LIỆU. (Trên bàn là những thứ con chuẩn bị: kén, tơ, sợi, thuốc nhuộm.) Con chọn dùng loại nào cho thử thách?")], [
    ch("Nguyên liệu tốt nhất con đã chuẩn bị (cần tay nghề mới dùng hết).", ["ClearFlags:Fest.Mat.", "SetFlag:Fest.Mat.High", "Progress:Q43.Select", "Notify:Chọn nguyên liệu tốt nhất – ra bàn nhuộm ở sân khấu"]),
    ch("Nguyên liệu vừa phải, chắc tay.", ["ClearFlags:Fest.Mat.", "SetFlag:Fest.Mat.Mid", "Progress:Q43.Select", "Notify:Chọn nguyên liệu vừa phải – ra bàn nhuộm ở sân khấu"]),
    ch("Nguyên liệu bình thường.", ["ClearFlags:Fest.Mat.", "SetFlag:Fest.Mat.Low", "Progress:Q43.Select", "Notify:Chọn nguyên liệu thường – ra bàn nhuộm ở sân khấu"]),
])
d("CHIEF_F_PHASE_HINT", [
    (CHIEF, "Bốn bàn ở sân khấu: nhuộm, dệt, may, rồi lên sàn diễn. Làm lần lượt. Bình tĩnh – con đã luyện đủ rồi.")])
d("FEST_FAIL_PHASE", [
    (CHIEF, "(Âm nhạc chậm dần. Cháu đứng đó, nhận ra mình đã không giữ được tay nghề ở bước này.) Không sao. Con đã đi được rất xa."),
    (CHIEF, "Nhưng con vẫn còn một cơ hội. Kỹ năng, nguyên liệu và bộ trang phục của con đều còn đó."),
], [
    ch("TRY AGAIN – Thử lại từ sáng ngày hội.", ["FestivalReset"]),
])
d("FEST_FAIL", [
    (CHIEF, "(Màn trình diễn kết thúc. Tiếng vỗ tay thưa thớt.) Tổng điểm của con: {Score:Final.Total}/100 – chưa đủ."),
    (CHIEF, "Chế tác {Score:Final.Craft} · Thiết kế {Score:Final.Fashion} · Văn hóa {Score:Final.Culture} · Thiện cảm {Score:Final.Rel} · Trình diễn {Score:Final.Perf}."),
    (CHIEF, "Không sao. Con đã đi được rất xa. Hãy dùng ngày hội này để học thêm – và thử lại."),
], [
    ch("TRY AGAIN – Thử lại từ sáng ngày hội.", ["FestivalReset"]),
])
d("FEST_SUCCESS", [
    (CHIEF, "(Người dân vỗ tay. Các nghệ nhân đứng lên.) FESTIVAL SUCCESS! Tổng điểm của con: {Score:Final.Total}/100 – xếp hạng {Rank}."),
    (CHIEF, "Chế tác {Score:Final.Craft} · Thiết kế {Score:Final.Fashion} · Văn hóa {Score:Final.Culture} · Thiện cảm {Score:Final.Rel} · Trình diễn {Score:Final.Perf}."),
    (CHIEF, "Con đã làm được. Nhưng điều quan trọng nhất không phải là điểm số. Con đã hiểu được vì sao một sợi tơ nhỏ bé…"),
    (CHIEF, "…có thể mang theo câu chuyện của cả một vùng đất."),
    (SILK + " · " + DYER + " · " + WEAV, "(Người nuôi tằm, người ươm tơ, người se tơ, người nhuộm, người dệt, người may – tất cả đứng cùng nhau, bên cạnh con.)"),
], actions=["SetFlag:Fest.Done", "EndDay:D_END"])
for name, speaker, text, objective in (
        ("SIGN_MARKET", "CHỢ LỤA (Silk Market)", "Các gian hàng bày tơ, sợi, vải và trang phục. Mỗi món đều mang dấu tay của một nghệ nhân – và một hoa văn riêng.", "Market"),
        ("SIGN_CRAFT", "KHU NGHỆ NHÂN (Crafting Area)", "Các nghệ nhân trình diễn: ươm tơ, se sợi, nhuộm, dệt, may. Khách đứng xem và hỏi han – nghề này lại được truyền thêm một thế hệ.", "Crafting"),
        ("SIGN_CULTURE", "KHU VĂN HÓA (Cultural Area)", "Lịch sử làng nghề, ý nghĩa các hoa văn, trang phục truyền thống và quy trình tạo lụa, được kể lại qua từng tấm bảng gỗ.", "Cultural")):
    d(name, [(speaker, text)], actions=["Progress:Q41.%s" % objective])

# ---------------------------------------------------------------- Quests
quests = [
    {"Name": "Q30", "Title": "Chương 6 – Ba ngày chuẩn bị", "Objectives": [
        {"Id": "TalkChief", "Text": "Gặp bà Xuân ở bảng thông báo của làng", "Target": 1, "TrackItem": "None"}],
     "OnComplete": ["Notify:Ba ngày tới không có nhiệm vụ bắt buộc – bạn tự quyết định chuẩn bị gì"]},
    {"Name": "Q33", "Title": "Nhiệm vụ phụ – Sợi tơ hoàn hảo (The Perfect Thread)", "Objectives": [
        {"Id": "PerfectThread", "Text": "Luyện se tơ đạt 5★ ở nhà luyện nghề", "Target": 1, "TrackItem": "None"}],
     "OnComplete": ["SetFlag:Recipe.RareSilk", "AddRel:NPC_SilkMaster:5", "AddSkill:Spin:30", "Notify:Ông Khánh tặng bạn Công thức tơ hiếm (Rare Silk Recipe)"]},
    {"Name": "Q34", "Title": "Nhiệm vụ phụ – Hoa văn đã thất lạc (The Lost Pattern)", "Objectives": [
        {"Id": "AskElder", "Text": "Hỏi bà Xuân về hoa văn cổ đã thất truyền", "Target": 1, "TrackItem": "None"},
        {"Id": "FindFragment", "Text": "Tìm mảnh hoa văn cổ ở căn nhà cũ gần rìa rừng", "Target": 1, "TrackItem": "PAT_ANCIENT_FRAGMENT_001"},
        {"Id": "ReturnTailor", "Text": "Mang mảnh hoa văn về cho cô Lụa", "Target": 1, "TrackItem": "None"}],
     "OnComplete": ["SetFlag:Unlock.AncientPattern", "ClearFlag:Gather.On", "AddRel:NPC_WeaverTailor:5", "AddCulture:6",
                    "Notify:Mở khóa Hoa văn cổ (Ancient Pattern) – nói với cô Lụa để dùng cho trang phục"]},
    {"Name": "Q40", "Title": "Ngày hội Tơ Lụa", "Objectives": [
        {"Id": "TalkChief", "Text": "Gặp bà Xuân ở quảng trường lễ hội", "Target": 1, "TrackItem": "None"}], "OnComplete": []},
    {"Name": "Q41", "Title": "Dạo quanh lễ hội", "Objectives": [
        {"Id": "Market", "Text": "Xem Chợ lụa (Silk Market)", "Target": 1, "TrackItem": "None"},
        {"Id": "Crafting", "Text": "Xem Khu nghệ nhân (Crafting Area)", "Target": 1, "TrackItem": "None"},
        {"Id": "Cultural", "Text": "Xem Khu văn hóa (Cultural Area)", "Target": 1, "TrackItem": "None"}],
     "OnComplete": ["AddCulture:8", "SetFlag:Need.NPC_Chief", "Notify:Lễ hội sắp đến phần cuối – hãy gặp bà Xuân", "StartQuest:Q42"]},
    {"Name": "Q42", "Title": "Thử thách cuối cùng", "Objectives": [
        {"Id": "TalkChief", "Text": "Gặp bà Xuân để nhận Thử thách Tơ Lụa", "Target": 1, "TrackItem": "None"}], "OnComplete": ["StartQuest:Q43"]},
    {"Name": "Q43", "Title": "THỬ THÁCH TƠ LỤA – The Master's Challenge", "Objectives": [
        {"Id": "Select", "Text": "Phase 1 – Chọn nguyên liệu (nói chuyện với bà Xuân)", "Target": 1, "TrackItem": "None"},
        {"Id": "Dye", "Text": "Phase 2 – Nhuộm đúng màu Đỏ lễ hội (bàn nhuộm ở sân khấu)", "Target": 1, "TrackItem": "None"},
        {"Id": "Weave", "Text": "Phase 3 – Dệt hoàn hảo (khung dệt ở sân khấu)", "Target": 1, "TrackItem": "None"},
        {"Id": "Tailor", "Text": "Phase 4 – May trang phục cuối cùng (bàn may ở sân khấu)", "Target": 1, "TrackItem": "None"},
        {"Id": "Perform", "Text": "Phase 5 – Trình diễn trên sân khấu", "Target": 1, "TrackItem": "None"}],
     "OnComplete": ["FinalScore"]},
]


# ---------------------------------------------------------------- Crafts
def rounds(labels, good, perfect, speed, low="Hơi sớm – chưa chuẩn", high="Hơi muộn – chưa chuẩn"):
    return [{"Label": s, "Centre": -1, "GoodHalf": good, "PerfectHalf": perfect, "Speed": speed + 0.05 * i, "TooLow": low, "TooHigh": high}
            for i, s in enumerate(labels)]


def craft(name, title, hint, require, block, take, rnds, skill, slot, fail, result, done, on_fail):
    return {"Name": name, "Title": title, "Hint": hint, "RequireFlags": require, "BlockFlags": block, "TakeItems": take,
            "Rounds": rnds, "Skill": skill, "SlotCost": slot, "FailBelow": fail, "ResultText": result, "OnDone": done, "OnFail": on_fail}


PRAC_HINT = "Luyện tay nghề: nhấn [E] khi kim trong vùng vàng. Mỗi lần luyện tốn 1 lượt hoạt động và cộng EXP kỹ năng.  ·  [Esc] rời đi"
crafts = []
for key, title, skill, labels in (
        ("PracCare", "Luyện nuôi tằm", "Care", ["Cho ăn đúng lúc", "Mở cửa thông gió", "Phun sương", "Dọn khay"]),
        ("PracReel", "Luyện ươm tơ", "Reel", ["Tìm đầu sợi", "Giữ lực kéo", "Giữ tốc độ", "Nối sợi đứt"]),
        ("PracSpin", "Luyện se tơ", "Spin", ["Nhịp guồng", "Giữ sợi", "Se đều", "Thu sợi"]),
        ("PracDye", "Luyện nhuộm", "Dye", ["Nhiệt độ", "Lượng thuốc", "Thời gian", "Phơi màu"]),
        ("PracWeave", "Luyện dệt", "Weave", ["Đưa thoi", "Giữ nhịp", "Giữ độ căng", "Chọn vị trí"]),
        ("PracTailor", "Luyện may", "Tailor", ["Cắt theo mẫu", "May đường chính", "Thêu hoa văn", "Kiểm tra"])):
    done = ["ProgressIfStars:Q33.PerfectThread:PracSpin:5"] if key == "PracSpin" else []
    crafts.append(craft(key, title, PRAC_HINT, ["Free.Days"], [], [], rounds(labels, 0.11, 0.04, 0.9), skill, 1, 0,
                        "LUYỆN XONG · Chất lượng {0} ({1}/100)", done, []))

FAIL_PHASE = ["SetFlag:Fest.Failed", "Dialogue:FEST_FAIL_PHASE"]
crafts.append(craft("ChDye", "Bàn nhuộm – Festival Red", "PHASE 2: tạo đúng màu Đỏ lễ hội. Sai quá nhiều là thất bại.  ·  [Esc] rời đi",
                    ["Quest.Q43.Active", "Obj.Q43.Select"], ["Obj.Q43.Dye"], [],
                    [{"Label": s, "Centre": -1, "GoodHalf": 0.115, "PerfectHalf": 0.045, "Speed": 0.9 + 0.05 * i,
                      "TooLow": "Thấp quá – màu đỏ không đủ sâu", "TooHigh": "Cao quá – đỏ bị sẫm cháy"}
                     for i, s in enumerate(["Nhiệt độ", "Lượng thuốc nhuộm", "Thời gian ngâm"])],
                    "Dye", 0, 45, "FESTIVAL RED · Chất lượng {0} ({1}/100)", ["Progress:Q43.Dye"], FAIL_PHASE))
crafts.append(craft("ChWeave", "Khung dệt sân khấu", "PHASE 3: dệt nhanh hơn, hoa văn phức tạp hơn. Giữ độ căng sợi vừa phải.  ·  [Esc] rời đi",
                    ["Quest.Q43.Active", "Obj.Q43.Dye"], ["Obj.Q43.Weave"], [],
                    [{"Label": "Nhịp %d – giữ độ căng" % (i + 1), "Centre": -1, "GoodHalf": 0.105, "PerfectHalf": 0.04, "Speed": 1.0 + 0.04 * i,
                      "TooLow": "Sợi chùng – Thread Break", "TooHigh": "Căng quá – Fabric Damage"} for i in range(8)],
                    "Weave", 0, 45, "DỆT HOÀN HẢO · Chất lượng {0} ({1}/100)", ["Progress:Q43.Weave"], FAIL_PHASE))
crafts.append(craft("ChTailor", "Bàn may sân khấu", "PHASE 4: làm hết các bước trong thời gian giới hạn: cắt, may, hoa văn, phụ kiện, kiểm tra cuối.  ·  [Esc] rời đi",
                    ["Quest.Q43.Active", "Obj.Q43.Weave"], ["Obj.Q43.Tailor"], [],
                    [{"Label": s, "Centre": -1, "GoodHalf": 0.115, "PerfectHalf": 0.045, "Speed": 0.95 + 0.05 * i,
                      "TooLow": "Làm vội – lỗi nhỏ", "TooHigh": "Làm chậm – lỗi nhỏ"}
                     for i, s in enumerate(["Cắt", "May", "Hoa văn", "Phụ kiện", "Kiểm tra cuối"])],
                    "Tailor", 0, 45, "TRANG PHỤC CUỐI CÙNG · Chất lượng {0} ({1}/100)", ["Progress:Q43.Tailor"], FAIL_PHASE))
crafts.append(craft("ChStage", "Sàn diễn – Festival Performance", "PHASE 5: bước lên sân khấu và trình diễn theo nhịp.  ·  [Esc] rời đi",
                    ["Quest.Q43.Active", "Obj.Q43.Tailor"], ["Obj.Q43.Perform"], [],
                    [{"Label": s, "Centre": -1, "GoodHalf": 0.14, "PerfectHalf": 0.05, "Speed": 0.85 + 0.05 * i,
                      "TooLow": "Lệch nhịp – hơi sớm", "TooHigh": "Lệch nhịp – hơi muộn"}
                     for i, s in enumerate(["Đi bước", "Xoay người", "Tạo dáng", "Giới thiệu hoa văn", "Tạo dáng cuối"])],
                    "Tailor", 0, 0, "TRÌNH DIỄN XONG · Chất lượng {0} ({1}/100)", ["Progress:Q43.Perform"], []))

# ---------------------------------------------------------------- Day cards
SKILL_LINE = ("Tay nghề: Nuôi tằm Lv{Skill:Care} · Ươm tơ Lv{Skill:Reel} · Se tơ Lv{Skill:Spin} · "
              "Nhuộm Lv{Skill:Dye} · Dệt Lv{Skill:Weave} · May Lv{Skill:Tailor}")
days = [
    {"Name": "D6_1", "Title": "NGÀY CHUẨN BỊ 1/3 – KẾT THÚC",
     "Lines": [SKILL_LINE, "Hiểu biết văn hóa: {Culture}", "Còn 2 ngày nữa là Lễ hội Tơ Lụa"],
     "SetFlags": ["SetFlag:Day6.1.Complete"],
     "OnClose": ["ClearFlag:Fday.1", "SetFlag:Fday.2", "ClearFlags:Lesson.", "SetSlots:4", "SetFlag:Need.NPC_Chief"],
     "Morning": "NGÀY CHUẨN BỊ 2/3 – Một số nghệ nhân giao việc khó hơn"},
    {"Name": "D6_2", "Title": "NGÀY CHUẨN BỊ 2/3 – KẾT THÚC",
     "Lines": [SKILL_LINE, "Hiểu biết văn hóa: {Culture}", "Còn 1 ngày nữa là Lễ hội Tơ Lụa – ngày mai là cơ hội chuẩn bị cuối cùng"],
     "SetFlags": ["SetFlag:Day6.2.Complete"],
     "OnClose": ["ClearFlag:Fday.2", "SetFlag:Fday.3", "ClearFlags:Lesson.", "SetSlots:4", "SetFlag:Need.NPC_Chief"],
     "Morning": "NGÀY CHUẨN BỊ 3/3 – Cả làng đang bận rộn chuẩn bị. Hôm nay là cơ hội cuối cùng"},
    {"Name": "D6_3", "Title": "NGÀY CHUẨN BỊ 3/3 – HOÀN THÀNH",
     "Lines": [SKILL_LINE, "Hiểu biết văn hóa: {Culture}", "Bộ trang phục cuối cùng đã đặt cạnh giường. Ngoài cửa sổ, cả làng sáng đèn.",
               "TOMORROW – THE SILK FESTIVAL"],
     "SetFlags": ["SetFlag:Day6.3.Complete"],
     "OnClose": ["ClearFlag:Free.Days", "ClearFlag:Fday.3", "SetFlag:Festival.Day", "SetFlag:Free.Done", "ClearFlags:Lesson.",
                 "StartQuest:Q40", "SetFlag:Need.NPC_Chief"],
     "Morning": "06:00 – NGÀY HỘI TƠ LỤA · Làng hoàn toàn thay đổi. Hãy gặp bà Xuân ở quảng trường"},
    {"Name": "D_END", "Title": "THE END",
     "Lines": ["Điểm tổng: {Score:Final.Total}/100 – xếp hạng {Rank}", "Một sợi tơ nhỏ bé có thể mang theo câu chuyện của cả một vùng đất.",
               "Cảm ơn bạn đã đồng hành cùng Làng Tơ Lụa."],
     "SetFlags": ["SetFlag:Game.Complete"], "OnClose": [], "Morning": "THE END"},
]

# The three free preparation days were cut: the Day 5 card leads straight into the festival morning. Everything that only
# served them (chief menus, artisan lessons, the Lost Pattern and Perfect Thread side quests, the practice hall, the day
# cards) is dropped here so the tables only hold what the game can reach.
DROP_PREFIX = ("CHIEF_D6_", "SILK_MENU", "SILK_LESSON", "DYER_MENU", "DYER_LESSON", "WEAVER_MENU", "WEAVER_LESSON", "SILK_D6_", "DYER_D6_",
               "WEAVER_D6_", "HELP_GENERIC", "FREE_BUSY", "Q30", "Q33", "Q34", "Prac", "D6_")
dialogue = [d_ for d_ in dialogue if not d_["Name"].startswith(DROP_PREFIX)]
rules = [r_ for r_ in rules if not r_["Name"].startswith(DROP_PREFIX) and not r_["Dialogue"].startswith(DROP_PREFIX)]
quests = [q_ for q_ in quests if not q_["Name"].startswith(DROP_PREFIX)]
crafts = [c_ for c_ in crafts if not c_["Name"].startswith(DROP_PREFIX)]
days = [d_ for d_ in days if not d_["Name"].startswith(DROP_PREFIX)]
# The festival can no longer be failed: finishing the five phases ends the game, the score only ranks it. Each phase
# also gets its own kind of mini-game instead of the shared timing bar.
FESTIVAL_MODES = {"ChDye": "Slider", "ChWeave": "Sequence", "ChTailor": "Trace", "ChStage": "Lanes"}
for c_ in crafts:
    c_["FailBelow"] = 0
    c_["OnFail"] = []
    c_["Mode"] = FESTIVAL_MODES.get(c_["Name"], "None")
    if c_["Mode"] == "Slider":
        c_["Hint"] = "PHASE 2: tạo đúng màu Đỏ lễ hội – giữ cả ba thanh trong vùng xanh. W/S chọn thanh, A/D chỉnh.  ·  [Esc] rời đi"
    elif c_["Mode"] == "Sequence":
        c_["Hint"] = "PHASE 3: dệt theo nhịp – bấm đúng phím đang sáng (W A S D hoặc phím mũi tên) trước khi hết giờ.  ·  [Esc] rời đi"
    elif c_["Mode"] == "Trace":
        c_["Hint"] = "PHASE 4: cắt theo mẫu – di chuột (hoặc giữ W/S) để kéo bám theo đường cắt.  ·  [Esc] rời đi"
    elif c_["Mode"] == "Lanes":
        c_["Hint"] = "PHASE 5: trình diễn – bấm A S W D (hoặc phím mũi tên) khi nốt chạm vạch vàng.  ·  [Esc] rời đi"
for name, data in (("dialogue", dialogue), ("talk_rules", rules), ("quests", quests), ("crafts", crafts), ("days", days)):
    with open(os.path.join(OUT, name + ".json"), "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
    print(name, len(data))
