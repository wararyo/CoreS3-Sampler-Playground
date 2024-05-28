# CoreS3 Sampler Playground

M5Stack CoreS3を使って音を生成して遊ぼう！

本体スピーカーや外部I2S DACからは音が出ません。  
生成された音を聞くには後述する手順を踏んでWAVファイルを作成する必要があります。

## 処理にかかる時間を計測する

main.cppの `ENABLE_PRINTING` を**false**にして書き込みます。  

M5Stack CoreS3のタッチパネルをタッチすると処理が始まります。  
終了後、処理にかかった時間が表示されます。

## WAVファイルを作成する

生成された音を聴くには、UARTを通してデータをPCに送信し、PCでWAVファイルを作成します。

main.cppの `ENABLE_PRINTING` を**true**にして書き込みます。  

PC上で `python3 SerialToWav.py --serial_name [シリアルポート名]` を実行します。  
SerialToWav.pyが待機状態に入ったことを確認したら、M5Stack CoreS3のタッチパネルをタッチして処理を開始させます。  
やがてCoreS3の画面上に `Processed.` と表示されるので、PC上で Ctrl+C を押してSerialToWav.pyを停止させます。

output.wavが作成されているので、好みの音声エディタで開きます。
