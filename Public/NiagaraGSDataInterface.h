#include "CoreMinimal.h"
#include "NiagaraDataInterface.h"
#include "NiagaraCommon.h"
#include "GaussianSplatData.h"
#include "GaussianSplatAsset.h"
#include "NiagaraGSDataInterfaceGPU.h"
#include "NiagaraDataInterfaceRW.h"
#include "NiagaraShaderParametersBuilder.h"
#include "NiagaraGSDataInterface.generated.h"

/**
 * Custom Niagara Data Interface exposing Gaussian Splat data streams
 * directly to GPU compute and CPU particles.
 */
UCLASS(EditInlineNew, Category = "Gaussian Splats", meta = (DisplayName = "Gaussian Splat Data Interface"))
class NIAGARAGS_API UNiagaraGSDataInterface : public UNiagaraDataInterface
{
    GENERATED_BODY()

public:
    // Splat Asset containing the point data
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaussian Splats")
    TObjectPtr<UGaussianSplatAsset> SplatAsset;

    // ── UNiagaraDataInterface interface ───────────────────────────
    virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions) override;
    virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction& OutFunc) override;
    virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override { return true; }
    virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;
    virtual bool Equals(const UNiagaraDataInterface* Other) const override;

    int32 GetSplatCount() const;

    virtual void PostInitProperties() override;
    virtual void PostLoad() override;

    // CPU VM bindings
    void GetSplatCount(FVectorVMExternalFunctionContext& Context);
    void GetSplatPosition(FVectorVMExternalFunctionContext& Context);
    void GetSplatScale(FVectorVMExternalFunctionContext& Context);
    void GetSplatOrientation(FVectorVMExternalFunctionContext& Context);
    void GetSplatColor(FVectorVMExternalFunctionContext& Context);
    void GetSplatOpacity(FVectorVMExternalFunctionContext& Context);

    // GPU Interface
    virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
    virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL) override;
    virtual void BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const override;
    virtual void SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const override;

    void UploadToGPU();

#if WITH_EDITOR
    virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
    static const FName Name_GetSplatCount;
    static const FName Name_GetSplatPosition;
    static const FName Name_GetSplatScale;
    static const FName Name_GetSplatOrientation;
    static const FName Name_GetSplatColor;
    static const FName Name_GetSplatOpacity;
}; 