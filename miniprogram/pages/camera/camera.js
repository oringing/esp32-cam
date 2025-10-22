// pages/camera/camera.js
Page({
  data: {
    camIp: "", // ESP32-CAM的IP地址
    showWebView: false, // 是否显示视频流
    webViewUrl: "", // WebView的URL
    capturedImage: "", // 拍照后的图片临时路径
  },

  onLoad: function (options) {
    // 页面加载时的初始化操作
  },

  // 设置ESP32-CAM IP地址
  setCamIp: function(e) {
    this.setData({
      camIp: e.detail.value
    });
  },

  // 显示视频流
  showStream: function() {
    if (!this.data.camIp) {
      wx.showToast({
        title: '请输入ESP32-CAM的IP地址',
        icon: 'none'
      });
      return;
    }
    
    // 构造视频流URL (ESP32-CAM默认端口是8080)
    const url = `http://${this.data.camIp}:8080/`;
    this.setData({
      showWebView: true,
      webViewUrl: url
    });
    
    wx.showToast({
      title: '正在加载视频流',
      icon: 'none'
    });
  },

  // 隐藏视频流
  hideStream: function() {
    this.setData({
      showWebView: false,
      webViewUrl: ""
    });
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