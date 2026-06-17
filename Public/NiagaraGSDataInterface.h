#pragma once

#include "CoreMinimal.h"
#include "NiagaraDataInterface.h"
#include "NiagaraCommon.h"
#include "GaussianSplatData.h"
#include "GaussianSplatAsset.h"
#include "NiagaraGSDataInterfaceGPU.h"    // proxy + shader param struct
#include "NiagaraDataInterfaceRW.h"
#include "NiagaraShaderParametersBuilder.h"
#include "NiagaraGSDataInterface.generated.h"



// Forward declare the GPU proxy — defined in Step 5
struct FNDIGaussianSplatProxy;

/**
 * Custom Niagara Data Interface that exposes Gaussian Splat data
 * to a Niagara GPU simulation.
 *
 * Usage in Niagara:
 *   - Add this NDI as a parameter on your emitter
 *   - Point it at a UGaussianSplatAsset
 *   - Use the exposed scratch pad functions to read per-splat
 *     position/scale/orientation/color/opacity by index
 *
 * CPU side (this file): function signatures, VM bindings, data upload
 * GPU side (Step 5):    HLSL definitions, structured buffer proxy
 */
UCLASS(EditInlineNew, Category = "Gaussian Splats",
    meta = (DisplayName = "Gaussian Splat Data Interface"))
    class NIAGARAGS_API UNiagaraGSDataInterface : public UNiagaraDataInterface
{
    GENERATED_BODY()

public:

    // ── Asset reference ───────────────────────────────────────────
    // Set this in the Niagara emitter's parameter panel to point
    // at the UGaussianSplatAsset you imported in Step 3.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaussian Splats")
    TObjectPtr<UGaussianSplatAsset> SplatAsset;

    // ── UNiagaraDataInterface interface ───────────────────────────

    // Register the functions we expose to Niagara scratch pads
    virtual void GetFunctions(
        TArray<FNiagaraFunctionSignature>& OutFunctions) override;

    // Bind CPU implementations to the function names registered above
    virtual void GetVMExternalFunction(
        const FVMExternalFunctionBindingInfo& BindingInfo,
        void* InstanceData,
        FVMExternalFunction& OutFunc) override;

    // Tell Niagara this NDI supports GPU compute sims
    // (the actual GPU impl comes in Step 5 — this must return true
    //  or Niagara will refuse to use us in a GPU emitter)
    virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override { return true; }

    // Required for Niagara to correctly copy/compare NDI instances
    virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;
    virtual bool Equals(const UNiagaraDataInterface* Other)          const override;

    // Called by Niagara when it needs to know how many splats exist
    // (used to drive SpawnCount in the emitter)
    int32 GetSplatCount() const;

    // ── PostInitProperties ────────────────────────────────────────
    virtual void PostInitProperties() override;

    // ── CPU function implementations ──────────────────────────────
    // These are bound via GetVMExternalFunction and called by the VM
    // when the emitter runs on CPU. Each reads from SplatAsset->SplatData.

    void GetSplatCount(FVectorVMExternalFunctionContext& Context);
    void GetSplatPosition(FVectorVMExternalFunctionContext& Context);
    void GetSplatScale(FVectorVMExternalFunctionContext& Context);
    void GetSplatOrientation(FVectorVMExternalFunctionContext& Context);
    void GetSplatColor(FVectorVMExternalFunctionContext& Context);
    void GetSplatOpacity(FVectorVMExternalFunctionContext& Context);


    // ── GPU interface (Step 5) ─────────────────────────────────────

    // Injects buffer variable declarations into Niagara's generated HLSL
    virtual void GetParameterDefinitionHLSL(
        const FNiagaraDataInterfaceGPUParamInfo& ParamInfo,
        FString& OutHLSL) override;

    // Injects the function body HLSL for each function name
    virtual bool GetFunctionHLSL(
        const FNiagaraDataInterfaceGPUParamInfo& ParamInfo,
        const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo,
        int                                          FunctionInstanceIndex,
        FString& OutHLSL) override;

    // Required — tells Niagara we're using the new (non-legacy) binding path
    //virtual bool UseLegacyShaderBindings() const override { return false; }

    // Declares the layout of our shader parameter struct to Niagara
    virtual void BuildShaderParameters(
        FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const override;

    // Called every frame on the render thread — binds our GPU buffers
    // into the shader parameter struct slots
    virtual void SetShaderParameters(
        const FNiagaraDataInterfaceSetShaderParametersContext& Context) const override;

    // Triggers the CPU→GPU upload when asset is assigned or changed
    void UploadToGPU();

    virtual void PostLoad() override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(
        struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:

    // String names for each function — defined once here so we can't
    // have a typo mismatch between GetFunctions and GetVMExternalFunction
    static const FName Name_GetSplatCount;
    static const FName Name_GetSplatPosition;
    static const FName Name_GetSplatScale;
    static const FName Name_GetSplatOrientation;
    static const FName Name_GetSplatColor;
    static const FName Name_GetSplatOpacity;
};
