1. Mở dự án bằng **Arduino IDE**.
2. Cài đặt các thư viện cần thiết:
   - `Blynk` (by Volodymyr Shymanskyy)
   - `DHT sensor library` (by Adafruit)
   - `LiquidCrystal_I2C`
3. Mở file `config.h` (hoặc file `.ino`) và điền thông tin:
   - `auth`: Mã Blynk Auth Token từ Dashboard Blynk.
   - `ssid`: Tên Wi-Fi.
   - `pass`: Mật khẩu Wi-Fi.
5. Chọn Board **ESP32 Dev Module** và tiến hành Upload code.
