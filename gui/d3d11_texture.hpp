#pragma once

#include <d3d11.h>
#include <imgui.h>


#include <opencv2/core/core.hpp>

class D3D11Texture {
    public:
    void init(ID3D11Device* device);

    void upload(const cv::Mat& bgrImage);

    void release();

    bool valid() const {return srv_ != nullptr;}

    ImTextureID view() const {return (ImTextureID)(intptr_t)srv_;}

    int width() const {return width_;}
    int height() const {return height_;}


private:
    ID3D11Device* device_ = nullptr;
    ID3D11Texture2D* texture_ = nullptr;
    ID3D11ShaderResourceView* srv_ = nullptr;

    int width_ = 0;
    int height_ = 0;
};

