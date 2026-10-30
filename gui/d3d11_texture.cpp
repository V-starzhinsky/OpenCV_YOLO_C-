#include "d3d11_texture.hpp"

#include <opencv2/imgproc.hpp>


void D3D11Texture::init(ID3D11Device* device) {
    device_ = device;
}

void D3D11Texture::release() {
    if (srv_) { srv_->Release(); srv_ = nullptr; }
    if (texture_) { texture_->Release(); texture_ = nullptr; }
    width_ = height_ = 0;
}

void D3D11Texture::upload(const cv::Mat& bgrImage) {
    if (!device_ || bgrImage.empty())  return;
    release();

    cv::Mat bgra;
    if (bgrImage.channels() == 3) {
        cv::cvtColor(bgrImage, bgra, cv::COLOR_BGR2BGRA);
    }else if (bgrImage.channels() == 4) {
        bgra = bgrImage;
    }else {
        cv::cvtColor(bgrImage, bgra, cv::COLOR_GRAY2BGRA);
    }

    if (!bgra.isContinuous()) {
        bgra = bgra.clone();
    }

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = static_cast<UINT>(bgra.cols);
    desc.Height = static_cast<UINT>(bgra.rows);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;



    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;


    D3D11_SUBRESOURCE_DATA initData{};
    initData.pSysMem = bgra.data;
    initData.SysMemPitch = static_cast<UINT>(bgra.step[0]);
    initData.SysMemSlicePitch = 0;


    ID3D11Texture2D* texture = nullptr;
    HRESULT hr = device_->CreateTexture2D(&desc, &initData, &texture);
    if (FAILED(hr) || !texture ) return;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    ID3D11ShaderResourceView* srv = nullptr;
    hr = device_->CreateShaderResourceView(texture, &srvDesc, &srv);
    if (FAILED(hr) || !srv ) {
        texture->Release();
        return;
    }


    texture_ = texture;
    srv_ = srv;
    width_ = bgra.cols;
    height_ = bgra.rows;

}