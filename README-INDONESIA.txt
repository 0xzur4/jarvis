================================================================
JARVIS v4 - Asisten AI Desktop untuk Windows
================================================================
Jarvis adalah aplikasi desktop native (C++ / Win32 API, tanpa runtime
tambahan) yang bisa diajak bicara: kamu ngomong -> dia menjawab dengan
suara, dengan wajah pixel animasi.

Wajahnya MELAYANG transparan di desktop (tanpa jendela, tanpa tombol)
dan mikrofonnya SELALU mendengarkan - tidak perlu klik apa pun.

BARU DI v4:
- PEMILIHAN SUARA (voice): pilih suara Jarvis sesukamu di Pengaturan
  API (misalnya suara cewek). Dropdown menampilkan semua voice TTS
  yang terinstal di Windows-mu.

BARU DI v3:
- STT diganti ke Whisper (multilingual): BAHASA INDONESIA SEKARANG
  DIDUKUNG! Pilih bahasa di setup: Indonesia / English / Otomatis.
- Perintah "buka ..." jauh lebih pintar: kenal "visual studio code",
  "vs code", "kalkulator", dll, plus resolusi via registry App Paths.
  Kalau gagal membuka, Jarvis MENGATAKANNYA (tidak gagal diam-diam).

CARA KERJA SINGKAT: mic (selalu nyala) -> VAD -> Whisper (STT lokal,
multilingual) -> API Atria (otak AI, streaming) -> SAPI Windows
(TTS, voice dipilih otomatis sesuai bahasa) -> wajah animasi.
Selama Jarvis berbicara, mic otomatis DIHENTIKAN SEMENTARA agar
suaranya sendiri tidak memicu pendengaran, lalu lanjut lagi.

----------------------------------------------------------------
KEBUTUHAN
----------------------------------------------------------------
- Windows 10 / 11 64-bit
- Mikrofon (untuk perintah suara)
- Koneksi internet (untuk API Atria)
- API key Atria (lihat bawah)
- RAM: disarankan 4 GB+ (model Whisper base ~150 MB, butuh ±500 MB
  memori saat berjalan)

----------------------------------------------------------------
CARA INSTAL
----------------------------------------------------------------
1. Ekstrak file ZIP ini ke folder mana saja, misalnya:
   C:\Jarvis\
   Isi folder harus seperti ini:
     jarvis.exe
     models\whisper-base.bin   (±148 MB - JANGAN dihapus/diubah)
     README-INDONESIA.txt (file ini)
   JANGAN pindahkan jarvis.exe keluar dari folder ini sendirian -
   dia butuh folder models\ di sebelahnya.
   (Paket v4 TIDAK lagi butuh libvosk.dll / DLL MinGW - semua sudah
   di dalam jarvis.exe.)

2. Jalankan jarvis.exe (klik 2x). TIDAK perlu "Run as administrator".

3. Saat pertama dijalankan, muncul jendela SETUP. Isi:
   - API Key    : kunci API dari console Atria (cara dapat: bawah)
   - Base URL   : biarkan default https://api.atria-asi.ai/v1
   - Model      : biarkan default Atria-Dawn-Preview
   - Microphone : PILIH microphone yang mau dipakai dari dropdown
   - Bahasa     : PILIH "Indonesia", "English", atau "Otomatis"
                  (deteksi bahasa otomatis per ucapan)
   Klik "Test Koneksi" untuk memastikan key valid, lalu
   "Simpan & Mulai".
   API key disimpan TERENKRIPSI (DPAPI Windows) di:
   %APPDATA%\Jarvis\config.ini - tidak dalam bentuk plaintext.
   Pilihan microphone & bahasa juga tersimpan di file yang sama.

4. Jika microphone yang tersimpan tidak ditemukan saat Jarvis
   dibuka (mis. headset dicabut), muncul dialog "Pilih Microphone"
   untuk memilih ulang.

----------------------------------------------------------------
CARA MENDAPAT API KEY ATRIA
----------------------------------------------------------------
1. Buka https://api.atria-asi.ai (atau console Atria)
2. Login dengan akun Google
3. Masuk ke halaman API Key
4. Klik "Create"/"Buat kunci", beri nama mis. jarvis-desktop
5. SALIN key yang muncul (hanya ditampilkan sekali!)
6. Tempel ke kolom API Key di jendela setup Jarvis

----------------------------------------------------------------
CARA PAKAI (TANPA TOMBOL)
----------------------------------------------------------------
- Wajah Jarvis muncul melayang di kanan bawah layar. Cukup BICARA -
  tidak ada tombol "Bicara" lagi. Dia mendengarkan terus, dalam
  bahasa yang kamu pilih di setup.
- Selesai bicara, diam sebentar (±1 detik): Jarvis memproses lalu
  menjawab dengan suara. Wajahnya animasi saat bicara.
- INDIKATOR TITIK WARNA di kanan bawah wajah:
    HIJAU  = sedang mendengarkan -> silakan bicara
    KUNING = sedang berpikir (menghubungi AI)
    BIRU   = sedang menjawab (mic dijeda otomatis)
    ABU    = idle / standby
    MERAH  = mic dimatikan via menu
  Ekspresi wajah juga berubah mengikuti statusnya.

MENGATUR TAMPILAN:
- GESER: tahan klik kiri TEPAT pada gambar wajah, lalu drag.
  (Area transparan di sekitar wajah tembus klik - itu normal.)
- MENU: klik kanan TEPAT pada gambar wajah:
    "Pengaturan API..."   -> ganti API key / URL / model / mic / bahasa
    "Pilih Microphone..." -> ganti microphone kapan saja
    "Mic: Nyala/Mati"     -> toggle mic sementara (titik jadi merah)
    "Keluar"              -> tutup Jarvis
- Jendela selalu di atas (topmost) dan tidak muncul di taskbar;
  menutupnya hanya lewat menu "Keluar".

PERINTAH LOKAL (dijalankan langsung, tanpa lewat AI):
- "buka visual studio code" / "buka vs code" -> membuka VS Code
- "buka notepad", "buka chrome", "buka kalkulator",
  "buka spotify", "buka discord", "buka telegram", "buka word",
  "buka excel", "buka paint", "buka cmd", "buka powershell",
  "buka file explorer"  -> membuka aplikasi yang dimaksud
  (juga bisa pakai "open ..." dalam bahasa Inggris)
- "volume 50"                   -> atur volume 0-100
- "kunci" / "lock"              -> kunci layar (LockWorkStation)

Kata "buka" boleh di tengah kalimat: "tolong buka visual studio
code" juga bisa. Kalau aplikasinya TIDAK ketemu, Jarvis akan
bilang "Maaf, saya tidak bisa membuka ..." - bukan diam saja.

Selain perintah di atas, ucapanmu diteruskan ke AI dan dijawab
sesuai konteks. AI otomatis menjawab dalam bahasa yang kamu pakai.

----------------------------------------------------------------
MENGGANTI SUARA (VOICE) - BARU di v4
----------------------------------------------------------------
Kamu bisa memilih suara Jarvis sesukamu, misalnya suara cewek.
Klik kanan pada wajah Jarvis > "Pengaturan API..." > bagian
"Suara (voice)".

Dropdown menampilkan SEMUA voice TTS yang terinstal di Windows-mu,
dengan format "Nama (Bahasa, Gender)", contoh:
  "Microsoft Zira Desktop (English, Female)"

Pilihanmu disimpan dan selalu dipakai. Kalau voice yang kamu pilih
ternyata di-uninstall, Jarvis otomatis kembali ke pemilihan bahasa.

TIPS: daftar voice tergantung Windows masing-masing. Cara menambah
suara baru:
- Settings > Time & Language > Language > Add a language >
  tambah bahasa (tiap language pack biasanya membawa voice-nya).
  Contoh: instal "English (United States)" untuk mendapatkan
  Microsoft Zira (suara cewek) atau Microsoft David (suara cowok).
- Atau: Settings > Accessibility > Narrator > Add voices.
Setelah instal, buka lagi Pengaturan API di Jarvis - voice baru
akan muncul di dropdown.

Catatan: saat kamu ganti pilihan "Bahasa bicara", Jarvis otomatis
memilih voice yang bahasanya cocok SEBAGAI default cerdas - tapi
kalau kamu sudah memilih voice manual, pilihanmu tidak akan
diubah.

----------------------------------------------------------------
SUARA TTS BAHASA INDONESIA
----------------------------------------------------------------
Jarvis memakai suara bawaan Windows (SAPI) dan OTOMATIS memilih
voice yang bahasanya cocok dengan pilihanmu (atau voice yang kamu
pilih manual di atas - pilihan manual selalu menang):
- Bahasa = Indonesia -> mencari voice id-ID (0x421)
- Bahasa = English   -> mencari voice en-US (0x409)
- Kalau voice yang cocok tidak ada, dipakai voice default.

Supaya jawaban berbahasa Indonesia terdengar natural, instal
language pack Bahasa Indonesia di Windows:
  Settings > Time & language > Language > Add a language >
  pilih "Bahasa Indonesia" > instal (sertakan opsi Speech).
Setelah itu restart Jarvis - dia otomatis memakai voice Indonesia.

----------------------------------------------------------------
KETERBATASAN v4 (JUJUR)
----------------------------------------------------------------
1. LATENSI REALISTIS 1-2 DETIK (+ waktu load model ±beberapa detik
   saat pertama bicara), bukan "tanpa delay" seperti di film.
   Urutan: deteksi suara -> Whisper STT -> AI streaming -> TTS.
   Model Whisper base lebih berat dari Vosk kecil, tapi akurasinya
   (terutama Indonesia) jauh lebih baik.

2. PAKET BESAR (±150-200 MB): sebagian besar adalah model Whisper
   base (148 MB). Ini harga akurasi multilingual.

3. "Otomatis (deteksi)" bahasa kadang salah deteksi pada ucapan
   SANGAT pendek (1-2 kata). Kalau kamu konsisten satu bahasa,
   pilih "Indonesia"/"English" saja agar stabil.

4. API Atria adalah model PREVIEW dengan batas 60 request/menit.
   Kalau kena limit (error 429), tunggu sebentar lalu coba lagi.

5. Selalu-listening = mic aktif terus. Kalau butuh privasi
   sementara, matikan mic lewat menu klik-kanan (titik merah).

----------------------------------------------------------------
FITUR YANG BELUM DITES (UNTESTED)
----------------------------------------------------------------
Aplikasi ini di-build dari Linux, jadi interaksi langsung dengan
Windows TIDAK bisa dites oleh pembuatnya. Yang sudah diverifikasi:
exe valid PE32+, semua dependensi DLL lengkap. Yang BELUM teruji di
Windows asli dan perlu kamu coba + laporkan:
- Akurasi Whisper bahasa Indonesia di mic asli (ini yang PENTING -
  tolong laporkan kalau masih sering salah dengar!)
- Tampilan transparan (color-key) di GDI asli - apakah wajah tampil
  bersih tanpa sisa magenta di tepinya
- Drag wajah & klik-kanan menu pada area wajah
- Pemilihan microphone (apakah semua device muncul di dropdown)
- Kualitas VAD di mikrofon asli - kalau Jarvis kepotong saat kamu
  masih bicara, atau malah tidak mulai mendengar, laporkan agar
  threshold-nya disetel ulang
- Kontrol volume via "volume 50"
- Membuka aplikasi via "buka visual studio code" dkk.
- Kunci layar via "kunci"
- Pemilihan voice TTS Indonesia otomatis

----------------------------------------------------------------
TROUBLESHOOTING
----------------------------------------------------------------
- "whisper model failed to load": pastikan folder models\ berisi
  whisper-base.bin (±148 MB) di sebelah jarvis.exe. Download ulang
  ZIP-nya kalau file corrupt.
- "Tidak ada microphone terdeteksi": colok/pasang mic, lalu buka
  menu klik-kanan > "Pilih Microphone...".
- Test koneksi "HTTP 401": API key salah / sudah dicabut. Buat baru.
- Test koneksi "HTTP 429": kena rate limit, tunggu 1 menit.
- Tidak ada suara jawaban: cek default playback device Windows dan
  volume; pastikan ada TTS voice terinstal
  (Settings > Time & language > Speech).
- Mikrofon tidak merespons: cek Settings > Privacy > Microphone,
  izinkan akses mikrofon untuk aplikasi desktop.
- Jarvis salah dengar terus: ganti Bahasa ke "Indonesia" (bukan
  Otomatis) di Pengaturan API, dan bicara agak pelan & jelas.
- Klik kanan tidak membuka menu: klik harus TEPAT pada gambar
  wajah (area transparan tembus ke jendela di bawahnya).
- Jendela setup muncul lagi / config hilang: config tersimpan per
  user Windows (%APPDATA%). Login sebagai user yang sama.
- "buka X" tapi aplikasi tidak terbuka: pastikan aplikasinya
  memang terinstal. Jarvis mencari via PATH dan registry App Paths.
  Kalau tetap gagal, dia akan mengatakannya.

----------------------------------------------------------------
PRIVASI
----------------------------------------------------------------
- Suara diproses LOKAL oleh Whisper (tidak dikirim ke mana-mana).
- Yang dikirim ke internet HANYA teks hasil transkrip, ke Base URL
  yang kamu isi (default api.atria-asi.ai).
- API key terenkripsi dengan DPAPI (hanya bisa dibuka oleh user
  Windows yang sama di mesin yang sama).

v4 - dibangun dengan C++17 + Win32 API murni, cross-compile MinGW.
Perubahan dari v2: STT Whisper multilingual (Indonesia didukung),
pilihan bahasa di setup, perintah "buka ..." lebih pintar,
pemilihan voice TTS otomatis sesuai bahasa.
