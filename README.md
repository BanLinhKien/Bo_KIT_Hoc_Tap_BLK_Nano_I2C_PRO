
# <p align="center">  Bộ KIT Học Tập Arduino BLK Nano I2C PRO   </p>

###  Giới Thiệu

- **Bộ KIT Học Tập Arduino BLK Nano I2C PRO** là bộ công cụ thực hành được xây dựng trên nền tảng vi điều khiển **Arduino Nano (ATmega328P)**, tập trung chuyên sâu vào giao thức giao tiếp **I2C (Inter-Integrated Circuit)**. 
- Bộ KIT được thiết kế dành cho người dùng *đã nắm vững kiến thức lập trình Arduino nền tảng* như GPIO, Timer, PWM, ADC, ngắt (Interrupt) và giao tiếp UART/Serial cơ bản. Nếu bạn chưa có những kiến thức này, chúng tôi khuyến nghị bắt đầu với [Bộ KIT Học Tập Arduino BLK Uno R3 Plus](https://banlinhkien.com/bo-kit-hoc-tap-arduino-uno-r3-blk-plus-p38419270.html) để xây dựng nền tảng trước khi tiếp cận bộ KIT này. Trên cơ sở đó, **Bộ KIT Học Tập Arduino I2C PRO BLK** sẽ giúp bạn làm chủ giao thức giao tiếp I2C, kết nối đồng thời nhiều thiết bị trên cùng một bus truyền thông và phát triển các dự án hệ thống nhúng có tính thực tiễn cao.

<p align="center"> <img src="https://raw.githubusercontent.com/theduong6168/BLKLab/refs/heads/main/image/linhkien.jpg"  width="500"/> </p>

- **Kết nối đơn giản, mở rộng dễ dàng:**
    - Chỉ cần 2 dây tín hiệu (SDA, SCL) để kết nối nhiều thiết bị trên cùng một bus.
    - Chuẩn 4 chân thống nhất (VCC – GND – SDA – SCL) cho toàn bộ module.
    - Ghép nối tiếp (daisy-chain) linh hoạt, không cần thay đổi mạch khi mở rộng.
```text
    🔴 VCC  (+5V)
    ⚫ GND  (0V)
    🟡 SDA  (Data)
    🔵 SCL  (Clock)
```
- **Học qua dự án thực hành trực quan:**
    - Học thêm các module mới thông qua các dự án trực quan 
    -  Trải nghiệm thao tác kết nối phần cứng đơn giản thông qua giao thức I2C
    - Vừa học vừa thấy kết quả ngay khi lắp ráp và chạy thử.
    - Từ giao tiếp một cảm biến đơn đến phối hợp nhiều cảm biến trên cùng một bus.


| Đối tượng khách hàng | Vấn đề gặp phải | Giải pháp từ bộ KIT |
|---|---|---|
| Đã có nền tảng lập trình Arduino, muốn nâng cao kỹ năng | Chưa biết cách tiếp cận module/cảm biến mới một cách hệ thống | Cung cấp các PRJ trực quan, thực hành theo từng module cụ thể |
| Từng làm dự án với nhiều cảm biến đơn lẻ | Đấu dây phức tạp, nhiều chân, dễ nhầm lẫn khi mở rộng | Chuẩn kết nối 4 dây I2C, đấu nối đơn giản, dễ mở rộng |
| Muốn ghép nhiều cảm biến trong cùng một dự án | Thiếu chân GPIO, khó quản lý nhiều thiết bị cùng lúc | Giao thức I2C cho phép nhiều cảm biến dùng chung 1 bus (SDA/SCL) |
| Định hướng phát triển dự án cá nhân/học thuật nâng cao | Thiếu nền tảng vững chắc về giao tiếp bus trước khi làm hệ thống phức tạp | Lộ trình PRJ từ cơ bản đến nâng cao, xây dựng tư duy hệ thống I2C |

### Các Dự Án Thực Hành

| Mã     | Dự Án                              |
| ------ | ---------------------------------- | 
| PRJ.01 | Bộ hẹn giờ đếm lùi di động         | 
| PRJ.02 | Thiết bị nhắc uống nước/uống thuốc | 
| PRJ.03 | Máy phân loại trái cây             | 
| PRJ.04 | Máy chiết rót mật ong              | 
| PRJ.05 | Thiết bị theo dõi sức khỏe         | 

### Danh Sách Linh Kiện Trong Bộ KIT

| STT | Linh kiện | Số lượng |
|:---:|-----------|:--------:|
| 1 | Arduino I2C BLK Shield | 1 |
| 2 | HUB Mở Rộng Giao Tiếp I2C | 1 |
| 3 | KIT Nano CH340G Cổng Type-C | 1 |
| 4 | Dây USB A - Type-C 50 cm | 1 |
| 5 | Màn hình OLED 1.5 inch SH1107 I2C | 1 |
| 6 | Rotary Encoder I2C | 1 |
| 7 | Module DS3231 + AT24C32 | 1 |
| 8 | Cảm biến màu RGB TCS34725 | 1 |
| 9 | Động cơ Servo MG90S | 1 |
| 10 | Module cảm biến đo nhịp tim MAX30102 | 1 |
| 11 | Dây trắng 4P XH2.54 - 4P, dài 10 cm | 1 |
| 12 | Dây Jumper Cái - Cái 4P 2.54 mm, dài 20 cm |5 |
| 13 | Pin Lithium 602030PL 300 mAh | 1 |
| 14 | Hộp đựng linh kiện 6 ngăn (16.5 × 12 × 6 cm) | 1 |

## FAQs

### 1. Tôi cần có kiến thức gì trước khi sử dụng bộ KIT?

Bộ KIT được thiết kế cho người đã có kiến thức Arduino cơ bản như GPIO, PWM, ADC, Interrupt và UART. Nếu bạn là người mới bắt đầu, hãy học với [Bộ KIT Học Tập Arduino BLK Uno R3 Plus](https://banlinhkien.com/bo-kit-hoc-tap-arduino-uno-r3-blk-plus-p38419270.html) trước.

### 2. Bộ KIT này khác gì so với Bộ KIT Arduino Plus?

[Bộ KIT Học Tập Arduino BLK Uno R3 Plus](https://banlinhkien.com/bo-kit-hoc-tap-arduino-uno-r3-blk-plus-p38419270.html) giúp bạn làm quen với lập trình Arduino và các ngoại vi cơ bản. **Bộ KIT Học Tập Arduino I2C PRO BLK** là cấp độ tiếp theo, tập trung vào giao tiếp I2C và xây dựng các hệ thống có nhiều cảm biến, module hoạt động đồng thời.
### 3. Tại sao bộ KIT sử dụng chuẩn kết nối I2C 4 dây?

Chuẩn 4 dây (VCC, GND, SDA, SCL) giúp việc lắp ráp nhanh chóng, giảm sai sót khi đấu nối và dễ dàng mở rộng thêm thiết bị mà không cần sử dụng nhiều chân GPIO.

### 4. Bộ KIT có tài liệu và mã nguồn đi kèm không?

Có.
Mỗi dự án đều đi kèm tài liệu hướng dẫn, sơ đồ đấu nối, mã nguồn mẫu và giải thích nguyên lý hoạt động để bạn dễ dàng học tập và phát triển.

### 5. Nếu gặp lỗi trong quá trình học thì sao?

Bạn có thể:
- Kiểm tra mục FAQ và hướng dẫn khắc phục lỗi trong tài liệu.
- Chạy chương trình I2C Scanner để kiểm tra kết nối.
- Đối chiếu sơ đồ đấu nối và mã nguồn mẫu.
- Liên hệ đội ngũ hỗ trợ kỹ thuật nếu vẫn chưa giải quyết được.
### 6. Bộ KIT này có giúp tôi làm được đồ án hoặc sản phẩm thực tế không?

Có. 
Mục tiêu của bộ KIT không chỉ là hướng dẫn cách sử dụng từng module riêng lẻ mà còn giúp bạn hiểu cách nhiều thiết bị phối hợp với nhau trên cùng một bus I2C. Đây là nền tảng quan trọng để phát triển các dự án nhúng hoàn chỉnh, từ đồ án môn học đến sản phẩm thực tế.


---



![BANLINHKIEN](https://pos.nvncdn.com/f2fe44-24897/store/20180126_gVLn1I1Irv2dz2XjhYDIshMM.png)

<img src="https://encrypted-tbn0.gstatic.com/images?q=tbn:ANd9GcTuMYP8t3RGESq5KDLPLjtqvTmeG_ZQzaz56Q&s" alt="Youtube" width="50" />   [**BanLinhKien.Vn**](https://www.youtube.com/@BanLinhKienVn)

<img src="https://encrypted-tbn0.gstatic.com/images?q=tbn:ANd9GcTvQHkkzWJBXyAHKcdVe2KQ3kYJvneBO-zUag&s" alt="Tiktok" width="50" />   [**BanLinhKien Retail**](https://www.tiktok.com/@banlinhkienretail?lang=vi-VN)

<img src="https://encrypted-tbn0.gstatic.com/images?q=tbn:ANd9GcRdgS0IxquKvMSPtIH1lRTPotJWkOBLT_KQ5g&s" alt="Facebook" width="50" />   [**Banlinhkien**](https://www.facebook.com/banlinhkienMH)

<img src="https://quyhyvong.com/wp-content/uploads/2021/11/Logo-Shopee.png" alt="Shopee" width="50" />   [**Banlinhkien.com Official Store**](https://shopee.vn/banlinhkien_mh)

<img src="https://cdn.advertisingvietnam.com/image/2022/04/28/1651134854323.png" alt="TiktokShop" width="50" />   [**BanLinhKien Retail**](https://www.tiktok.com/@banlinhkienretail?_t=8qfDAd26YlD&_r=1)

<img src="https://pos.nvncdn.com/f2fe44-24897/store/20180126_gVLn1I1Irv2dz2XjhYDIshMM.png" alt="Banlinhkien" width="50" />   [**BanLinhKien**](https://banlinhkien.com/)

