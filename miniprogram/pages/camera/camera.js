// pages/camera/camera.js
Page({
  data: {
    camIp: "", // ESP32-CAM的IP地址
    showStream: false, // 是否显示视频流
    streamImageUrl: "", // 视频流图像URL
    showWebView: false, // 保留原变量，但不再使用
    webViewUrl: "", // 保留原变量，但不再使用
    capturedImage: "", // 拍照后的图片临时路径
    streamTimer: null, // 定时器引用
    streamInterval: 1000, // 获取图像间隔(毫秒)
  },

  onLoad: function (options) {
    // 页面加载时的初始化操作
  },

  onUnload: function() {
    // 页面卸载时清除定时器
    this.stopStreamTimer();
  },

  // 设置ESP32-CAM IP地址
  setCamIp: function(e) {
    this.setData({
      camIp: e.detail.value
    });
  },

  // 显示视频流 - 改为启动图像轮询
  showStream: function() {
    if (!this.data.camIp) {
      wx.showToast({
        title: '请输入ESP32-CAM的IP地址',
        icon: 'none'
      });
      return;
    }
    
    this.setData({
      showStream: true,
      streamImageUrl: `http://${this.data.camIp}:8080/?action=stream` // 尝试常见的ESP32-CAM流地址
    });
    
    // 启动定时器获取图像帧
    this.startStreamTimer();
    
    wx.showToast({
      title: '正在加载视频流',
      icon: 'none'
    });
  },

  // 隐藏视频流
  hideStream: function() {
    this.setData({
      showStream: false,
      streamImageUrl: ""
    });
    this.stopStreamTimer();
  },

  // 启动流定时器 - 定期获取新的图像帧
  startStreamTimer: function() {
    this.stopStreamTimer(); // 先清除现有定时器
    
    const timer = setInterval(() => {
      if (this.data.camIp && this.data.showStream) {
        // 直接使用当前设置的streamImageUrl
        // 由于小程序限制，我们无法动态更新image的src，所以通过setData更新
        this.setData({
          streamImageUrl: `http://${this.data.camIp}:8080/?action=stream&timestamp=${Date.now()}`
        });
      }
    }, this.data.streamInterval);
    
    this.setData({ streamTimer: timer });
  },

  // 停止流定时器
  stopStreamTimer: function() {
    if (this.data.streamTimer) {
      clearInterval(this.data.streamTimer);
      this.setData({ streamTimer: null });
    }
  },

  // 拍照功能
  takePicture: function() {
    if (!this.data.camIp) {
      wx.showToast({
        title: '请输入ESP32-CAM的IP地址',
        icon: 'none'
      });
      return;
    }
    
    // 这里应该调用ESP32-CAM的拍照API或者通过MQTT发送拍照指令
    // 示例：通过HTTP请求获取图片
    const imageUrl = `http://${this.data.camIp}:8080/capture`;
    
    wx.request({
      url: imageUrl,
      method: 'GET',
      responseType: 'arraybuffer',
      success: (res) => {
        if (res.statusCode === 200) {
          // 将arraybuffer转换为base64格式
          const base64 = wx.arrayBufferToBase64(res.data);
          const imageSrc = 'data:image/jpeg;base64,' + base64;
          
          this.setData({
            capturedImage: imageSrc
          });
          
          wx.showToast({
            title: '拍照成功',
            icon: 'success'
          });
        } else {
          wx.showToast({
            title: '拍照失败',
            icon: 'none'
          });
        }
      },
      fail: (err) => {
        wx.showToast({
          title: '请求失败',
          icon: 'none'
        });
        console.error('拍照请求失败:', err);
      }
    });
  },

  // 保存图片到相册
  saveToAlbum: function() {
    if (!this.data.capturedImage) {
      wx.showToast({
        title: '没有可保存的图片',
        icon: 'none'
      });
      return;
    }
    
    // 保存图片到相册
    wx.saveImageToPhotosAlbum({
      filePath: this.data.capturedImage,
      success: () => {
        wx.showToast({
          title: '保存成功',
          icon: 'success'
        });
      },
      fail: (err) => {
        wx.showToast({
          title: '保存失败',
          icon: 'none'
        });
        console.error('保存图片失败:', err);
      }
    });
  },

  // 开始视频流传输（通过MQTT）
  startVideoStream: function() {
    // 这里应该通过MQTT发送开始视频流指令
    wx.showToast({
      title: '发送开始视频流指令',
      icon: 'none'
    });
    // 实际应用中需要通过MQTT发送 "start_stream" 消息到相应主题
  },

  // 停止视频流传输（通过MQTT）
  stopVideoStream: function() {
    // 这里应该通过MQTT发送停止视频流指令
    wx.showToast({
      title: '发送停止视频流指令',
      icon: 'none'
    });
    // 实际应用中需要通过MQTT发送 "stop_stream" 消息到相应主题
  }
});