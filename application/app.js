const express = require('express');
const path = require('path');

const app = express();
const port = 3000;

// 設置靜態文件夾,用於提供圖像文件
app.use(express.static(path.join(__dirname, 'public')));

// 根路由,提供HTML頁面
app.get('/', (req, res) => {
  res.sendFile(path.join(__dirname, 'index.html'));
});

// 新增的路由,用於獲取圖片
app.get('/get-image', (req, res) => {
  const imagePath = path.join(__dirname, 'public', 'captured_image.jpg');
  res.sendFile(imagePath);
});

app.listen(port, () => {
  console.log(`Server is running on port ${port}`);
});