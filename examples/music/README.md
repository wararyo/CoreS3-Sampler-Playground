# CapsuleSampler Music Example

M5Stack CoreS3を使って音楽を聴くことができます。  
You can listen to music by writing this example to M5Stack CoreS3.

## 操作方法

上下にスワイプ: 曲選択  
タッチ: 再生開始

Swipe up or down: Select song  
Touch: Play the selected song

## 処理にかかる時間を計測する

main.cppの `ENABLE_PRINTING` を**false**にして書き込みます。  

M5Stack CoreS3のタッチパネルをタッチすると処理が始まります。  
この時、生成された音が本体スピーカーから再生されます。   
終了後、処理にかかった時間が表示されます。

## WAVファイルを作成する

生成された音をPC上で聴くには、UARTを通してデータをPCに送信し、PCでWAVファイルを作成します。

main.cppの `ENABLE_PRINTING` を**true**にして書き込みます。  

PC上で `python3 SerialToWav.py --serial_name [シリアルポート名]` を実行します。  
SerialToWav.pyが待機状態に入ったことを確認したら、M5Stack CoreS3のタッチパネルをタッチして処理を開始させます。  
やがてCoreS3の画面上に `Processed.` と表示されるので、PC上で Ctrl+C を押してSerialToWav.pyを停止させます。

output.wavが作成されているので、好みの音声エディタで開きます。
