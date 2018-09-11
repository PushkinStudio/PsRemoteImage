// Copyright 2015-2018 Mail.Ru Group. All Rights Reserved.

#include "PsRemoteImagePrivatePCH.h"
#include "PsRemoteImage.h"
#include "PsRemoteImageLibrary.h"
#include "SImageWithThrobber.h"

#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/SecureHash.h"
#include "Misc/DateTime.h"
#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFilemanager.h"
#include "Framework/Application/SlateApplication.h"
#include "Brushes/SlateDynamicImageBrush.h"

#define LOCTEXT_NAMESPACE "PsRemoteImageModule"

UPsRemoteImage::UPsRemoteImage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bShowProgress = true;
	bWorking = false;
	bPendingWork = false;
}

#if WITH_EDITOR
const FText UPsRemoteImage::GetPaletteCategory()
{
	return LOCTEXT("Common", "Common");
}
#endif // WITH_EDITOR

void UPsRemoteImage::SetURL(FString InURL)
{
	UE_LOG(LogPsRemoteImage, Verbose, TEXT("%s %s"), *PS_FUNC_LINE, *InURL);
	
	URL = InURL;
	LoadImage(URL);
}

void UPsRemoteImage::SetShowProgress(bool bInShowProgress)
{
	UE_LOG(LogPsRemoteImage, Verbose, TEXT("%s %d"), *PS_FUNC_LINE, bInShowProgress);
	bShowProgress = bInShowProgress;
}

TSharedRef<SWidget> UPsRemoteImage::RebuildWidget()
{
	UE_LOG(LogPsRemoteImage, Verbose, TEXT("%s"), *PS_FUNC_LINE);
	SlateImage = SNew(SImageWithThrobber);
	return SlateImage.ToSharedRef();
}

void UPsRemoteImage::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	
	UE_LOG(LogPsRemoteImage, Verbose, TEXT("%s"), *PS_FUNC_LINE);
	SlateImage.Reset();
}

void UPsRemoteImage::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	UE_LOG(LogPsRemoteImage, Verbose, TEXT("%s"), *PS_FUNC_LINE);
	LoadImage(URL);
}

void UPsRemoteImage::LoadImage(FString InURL)
{
	if (InURL.IsEmpty())
	{
		LoadImageComplete();
		return;
	}
	
	UE_LOG(LogPsRemoteImage, Verbose, TEXT("%s %s"), *PS_FUNC_LINE, *InURL);
	
	if (bWorking)
	{
		bPendingWork = true;
		UE_LOG(LogPsRemoteImage, Warning, TEXT("%s work in progress, postponing image loading"), *PS_FUNC_LINE);
		return;
	}
	
	bWorking = true;
	
	if (SlateImage.IsValid())
	{
		SlateImage->SetShowProgress(bShowProgress && true);
	}
	
	const bool bImageCached = IsImageCached(InURL);
	if (bImageCached)
	{
		AsyncLoadCachedImage(InURL);
	}
	else
	{
		AsyncDownloadImage(InURL);
	}
}

void UPsRemoteImage::LoadImageComplete()
{
	ImageData.Empty();
	bWorking = false;
	
	if (SlateImage.IsValid())
	{
		SlateImage->SetShowProgress(false);
	}
	
	if (bPendingWork)
	{
		UE_LOG(LogPsRemoteImage, Verbose, TEXT("%s bPendingWork true, URL %s"), *PS_FUNC_LINE, *URL);
		bPendingWork = false;
		LoadImage(URL);
	}
	else
	{
		OnImageLoaded.Broadcast();
	}
}

bool UPsRemoteImage::IsImageCached(const FString& InURL) const
{
	const bool bCached = !InURL.IsEmpty() && FPaths::FileExists(GetCacheFilename(InURL));
	UE_LOG(LogPsRemoteImage, Verbose, TEXT("%s URL %s : %d"), *PS_FUNC_LINE, *InURL, bCached);
	
	return bCached;
}

FString UPsRemoteImage::GetCacheFilename(const FString& InURL) const
{
	if (InURL.IsEmpty())
	{
		UE_LOG(LogPsRemoteImage, Error, TEXT("%s empty URL"), *PS_FUNC_LINE);
		return FString();
	}
	
	const FString Hash = FMD5::HashAnsiString(*InURL);
	const FString Filename = FPaths::Combine(UPsRemoteImageLibrary::GetCacheDirectory(), FString::Printf(TEXT("%s.bin"), *Hash));
	UE_LOG(LogPsRemoteImage, Verbose, TEXT("%s %s : %s"), *PS_FUNC_LINE, *InURL, *Filename);
	return Filename;
}

void UPsRemoteImage::AsyncAddImageToCache(const FString &InURL, const TArray<uint8>& InImageData)
{
	UE_LOG(LogPsRemoteImage, Verbose, TEXT("%s %s"), *PS_FUNC_LINE, *InURL);
	
	Async<void>(EAsyncExecution::ThreadPool, [this, InURL, &InImageData]()
	{
		const FString CacheFilename = GetCacheFilename(InURL);
		const bool bWritten = FFileHelper::SaveArrayToFile(InImageData, *CacheFilename);
		if (bWritten)
		{
			IFileManager::Get().SetTimeStamp(*CacheFilename, FDateTime::UtcNow());
		}
		
		AsyncTask(ENamedThreads::GameThread, [this, InURL]()
		  {
			  LoadImageComplete();
		  });
	});
}

FString UPsRemoteImage::GetCacheDate(const FString& InURL) const
{
	const FString CacheFilename = GetCacheFilename(InURL);
	const FDateTime ModificationDate = IFileManager::Get().GetTimeStamp(*CacheFilename);
	return ModificationDate.ToHttpDate();
}

void UPsRemoteImage::RemoveImageFromCache(const FString& InURL)
{
	UE_LOG(LogPsRemoteImage, Verbose, TEXT("%s %s"), *PS_FUNC_LINE, *InURL);
	
	const FString CacheFilename = GetCacheFilename(InURL);
	const bool bDeleted = FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*CacheFilename);
	
	if (!bDeleted)
	{
		UE_LOG(LogPsRemoteImage, Error, TEXT("%s can't delete cached image"), *PS_FUNC_LINE);
	}
}

void UPsRemoteImage::AsyncLoadCachedImage(const FString& InURL)
{
	UE_LOG(LogPsRemoteImage, Verbose, TEXT("%s %s"), *PS_FUNC_LINE, *InURL);
	
	Async<void>(EAsyncExecution::ThreadPool, [this, InURL]()
	{
		const FString CacheFilename = GetCacheFilename(InURL);
		TArray<uint8> ReadImageData;
		const bool bReadSuccess = FFileHelper::LoadFileToArray(ReadImageData, *CacheFilename);
		if (bReadSuccess)
		{
			ImageData = ReadImageData;
			
			AsyncTask(ENamedThreads::GameThread, [this, InURL]()
			  {
				  LoadCachedImageSuccess(InURL);
			  });
		}
		else
		{
			AsyncTask(ENamedThreads::GameThread, [this, InURL]()
			  {
				  LoadCachedImageFailure(InURL);
			  });
		}
	});
}

void UPsRemoteImage::LoadCachedImageSuccess(const FString& InURL)
{
	const bool bLoadedImage = SetImageData(ImageData);
	
	UE_LOG(LogPsRemoteImage, Verbose, TEXT("%s %s bLoadedImage %d"), *PS_FUNC_LINE, *InURL, bLoadedImage);
	
	if (bLoadedImage)
	{
		if (SlateImage.IsValid())
		{
			SlateImage->SetShowProgress(false);
		}
		
		AsyncDownloadImageCheckCache(InURL);
	}
	else
	{
		UE_LOG(LogPsRemoteImage, Error, TEXT("%s can't load cached image, trying to download it"), *PS_FUNC_LINE);
		RemoveImageFromCache(InURL);
		AsyncDownloadImage(InURL);
	}
}

void UPsRemoteImage::LoadCachedImageFailure(const FString& InURL)
{
	UE_LOG(LogPsRemoteImage, Verbose, TEXT("%s %s"), *PS_FUNC_LINE, *InURL);
	
	RemoveImageFromCache(InURL);
	AsyncDownloadImage(InURL);
}

void UPsRemoteImage::AsyncDownloadImage(const FString& InURL)
{
	UE_LOG(LogPsRemoteImage, Verbose, TEXT("%s %s"), *PS_FUNC_LINE, *InURL);
	
	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UPsRemoteImage::DownloadImage_HttpRequestComplete);
	HttpRequest->SetURL(InURL);
	HttpRequest->SetVerb(TEXT("GET"));
	
	HttpRequest->ProcessRequest();
}

void UPsRemoteImage::DownloadImage_HttpRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (bWasSuccessful && Response.IsValid())
	{
		ImageData = Response->GetContent();
		
		const bool bLoadedImage = SetImageData(ImageData);
		UE_LOG(LogPsRemoteImage, Verbose, TEXT("%s bWasSuccessful true, bLoadedImage %d"), *PS_FUNC_LINE, bLoadedImage);
		if (bLoadedImage)
		{
			AsyncAddImageToCache(*Request->GetURL(), ImageData);
		}
	}
	else
	{
		UE_LOG(LogPsRemoteImage, Error, TEXT("%s failed to download image"), *PS_FUNC_LINE);
		LoadImageComplete();
	}
}

void UPsRemoteImage::AsyncDownloadImageCheckCache(const FString& InURL)
{
	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UPsRemoteImage::DownloadImageCheckCache_HttpRequestComplete);
	HttpRequest->SetURL(InURL);
	HttpRequest->SetVerb(TEXT("GET"));
	
	const FString CacheFileDate = GetCacheDate(InURL);
	UE_LOG(LogPsRemoteImage, Verbose, TEXT("%s %s CacheFileDate %s"), *PS_FUNC_LINE, *InURL, *CacheFileDate);
	HttpRequest->SetHeader(TEXT("If-Modified-Since"), CacheFileDate);
	
	HttpRequest->ProcessRequest();
}

void UPsRemoteImage::DownloadImageCheckCache_HttpRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (Response.IsValid())
	{
		if (Response->GetResponseCode() == 304)
		{
			UE_LOG(LogPsRemoteImage, Verbose, TEXT("%s 304 Not Modified"), *PS_FUNC_LINE);
			LoadImageComplete();
		}
		else
		{
			UE_LOG(LogPsRemoteImage, Verbose, TEXT("%s image modified on server (code %d)"), *PS_FUNC_LINE, Response->GetResponseCode());
			DownloadImage_HttpRequestComplete(Request, Response, bWasSuccessful);
		}
	}
	else
	{
		UE_LOG(LogPsRemoteImage, Error, TEXT("%s invalid response"), *PS_FUNC_LINE);
		LoadImageComplete();
	}
}

bool UPsRemoteImage::SetImageData(const TArray<uint8>& InImageData)
{
	if (SlateImage.IsValid())
	{
		SlateImage->SetImage(nullptr);
	}
	else
	{
		UE_LOG(LogPsRemoteImage, Verbose, TEXT("%s invalid SlateImage"), *PS_FUNC_LINE);
		return false;
	}
	
	Brush = CreateBrush(InImageData);
	
	if (Brush.IsValid())
	{
		SlateImage->SetImage(Brush.Get());
		return true;
	}
	else
	{
		UE_LOG(LogPsRemoteImage, Error, TEXT("%s invalid Brush"), *PS_FUNC_LINE);
		return false;
	}
}

TSharedPtr<FSlateDynamicImageBrush> UPsRemoteImage::CreateBrush(const TArray<uint8>& InImageData) const
{
	UE_LOG(LogPsRemoteImage, Verbose, TEXT("%s"), *PS_FUNC_LINE);
	
	TSharedPtr<FSlateDynamicImageBrush> BrushRet;
	
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	const EImageFormat ImageType = ImageWrapperModule.DetectImageFormat(InImageData.GetData(), InImageData.Num());
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageType);
	
	if (!ImageWrapper.IsValid())
	{
		UE_LOG(LogPsRemoteImage, Error, TEXT("%s can't create image wrapper"), *PS_FUNC_LINE);
		return nullptr;
	}
	else if (ImageWrapper->SetCompressed(InImageData.GetData(), InImageData.Num()))
	{
		const int32 BytesPerPixel = ImageWrapper->GetBitDepth();
		const TArray<uint8>* RawData = nullptr;
		
		if (ImageWrapper->GetRaw(ERGBFormat::BGRA, BytesPerPixel, RawData) && RawData && RawData->Num() > 0)
		{
			const FName ResourceName = FName(*FString::Printf(TEXT("PSRemoteImage_%s"), *FGuid::NewGuid().ToString()));
			
			if (FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(ResourceName, ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), *RawData))
			{
				BrushRet = MakeShareable(new FSlateDynamicImageBrush(ResourceName, FVector2D(ImageWrapper->GetWidth(), ImageWrapper->GetHeight())));
				return BrushRet;
			}
			else
			{
				UE_LOG(LogPsRemoteImage, Error, TEXT("%s can't generate resource"), *PS_FUNC_LINE);
				return nullptr;
			}
		}
		else
		{
			UE_LOG(LogPsRemoteImage, Error, TEXT("%s can't get raw data"), *PS_FUNC_LINE);
			return nullptr;
		}
	}
	else
	{
		UE_LOG(LogPsRemoteImage, Error, TEXT("%s can't load compressed data"), *PS_FUNC_LINE);
		return nullptr;
	}
}

#undef LOCTEXT_NAMESPACE
