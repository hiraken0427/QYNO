# 型拡張後の実装段階

`docs/types-roadmap.md` の新段階1〜6は0.26.15で完了した。
以後はその続きの新段階7から番号を付ける。旧段階7（unsigned整数）とは区別する。
各段階を検証・devへ1回pushした後、次の実装指示を待つ。

| 新段階 | バージョン | 内容 | 状態 |
| --- | --- | --- | --- |
| 7 | 0.26.16 | モジュールのpublic / private、公開APIの型検査、エディター対応 | 実装済み |
| 8 | 0.27.0 | 標準関数のusing統一、lib.size / lib.length、io.path基本9関数 | 実装済み |
| 9 | 0.27.1 | バイナリモードrb/wb/ab、read_bytes / write_bytes | 実装済み |

仕様: [モジュールの公開制御](module-visibility.md)、[標準ライブラリ0.27.0仕様](standard-library-0270-design.md)、
[バイナリファイル操作0.27.1仕様](binary-file-design.md)。
新段階10以降は、回復可能I/O・ファイル位置操作などを候補とし、実装指示で確定する。
