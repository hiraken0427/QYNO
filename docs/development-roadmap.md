# 型拡張後の実装段階

`docs/types-roadmap.md` の新段階1〜6は0.26.15で完了した。
以後はその続きの新段階7から番号を付ける。旧段階7（unsigned整数）とは区別する。
各段階を検証・devへ1回pushした後、次の実装指示を待つ。

| 新段階 | バージョン | 内容 | 状態 |
| --- | --- | --- | --- |
| 7 | 0.26.16 | モジュールのpublic / private、公開APIの型検査、エディター対応 | 実装済み |
| 8 | 0.27.0 | 標準関数のusing統一、lib.size / lib.length、io.path基本9関数 | 実装済み |
| 9 | 0.27.1 | バイナリモードrb/wb/ab、read_bytes / write_bytes | 実装済み |
| 10 | 0.27.1追加改訂（指定） | 独立enum、enum用match、型推論・入出力・エディターへの統合 | 実装済み |
| 11 | 0.27.2 | io.ErrorCode / io.Error / try_open、try/case/finallyと再送出、回復経路のclose検査 | 実装済み |
| 12 | 0.27.3 | fileのtell / seek / size / modified_at、標準timeの計測・待機・単位・Stopwatch・日時処理 | 実装済み |
| 13 | 0.27.4 | io.dir.current / change、io.path.absolute、ディレクトリのエラー分類 | 実装済み |
| 14 | 0.27.5 | mainのstr配列引数、(void)による引数なしの明示 | 実装済み |
| 15 | 0.27.6 | lib.randomの型名API・不等式範囲、独立Random、固定/OSシード、char/str | 実装済み |
| 16 | 0.27.7 | str / charのcontains・前後判定・find・trim・split・replace | 実装済み |

仕様: [モジュールの公開制御](module-visibility.md)、[標準ライブラリ0.27.0仕様](standard-library-0270-design.md)、
[バイナリファイル操作0.27.1仕様](binary-file-design.md)。
[列挙型と関連機能の仕様](enums-design.md)を参照。
[回復可能I/Oとtry制御文の仕様](recoverable-io-design.md)を参照。
新段階12の仕様は[ファイル位置とメタデータ](file-position-design.md)、[標準time](time-design.md)を参照。
新段階12は0.27.3で実装した。
新段階13〜15の仕様は[作業ディレクトリ・実行時引数・乱数](process-path-random-design.md)を参照。
新段階16は[文字列の標準メソッドの仕様](text-methods-design.md)を参照。
別の後続候補としてr+bの更新モード、部分転送を扱うtry_read / try_write等がある。
