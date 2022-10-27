// Copyright 2022 Jesus Bracho All Rights Reserved.

#include "RTSCamera.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Runtime/CoreUObject/Public/UObject/ConstructorHelpers.h"

URTSCamera::URTSCamera()
{
	PrimaryComponentTick.bCanEverTick = true;
	this->CameraBlockingVolumeTag = FName("OpenRTSCamera#CameraBounds");
	this->CollisionChannel = ECC_WorldStatic;
	this->DragExtent = 0.6f;
	this->EdgeScrollSpeed = 50;
	this->DistanceFromEdgeThreshold = 0.1f;
	this->EnableCameraLag = true;
	this->EnableCameraRotationLag = true;
	this->EnableDynamicCameraHeight = true;
	this->EnableEdgeScrolling = true;
	this->FindGroundTraceLength = 100000;
	this->MaximumZoomLength = 5000;
	this->MinimumZoomLength = 500;
	this->MoveSpeed = 50;
	this->RotateSpeed = 45;
	this->StartingYAngle = -45.0f;
	this->StartingZAngle = 0;
	this->ZoomCatchupSpeed = 4;
	this->ZoomSpeed = -200;

	// 绑定操作
	static ConstructorHelpers::FObjectFinder<UInputAction>
		MoveCameraXAxisFinder(TEXT("/OpenRTSCamera/Inputs/MoveCameraXAxis"));
	static ConstructorHelpers::FObjectFinder<UInputAction>
		MoveCameraYAxisFinder(TEXT("/OpenRTSCamera/Inputs/MoveCameraYAxis"));
	static ConstructorHelpers::FObjectFinder<UInputAction>
		RotateCameraAxisFinder(TEXT("/OpenRTSCamera/Inputs/RotateCameraAxis"));
	static ConstructorHelpers::FObjectFinder<UInputAction>
		TurnCameraLeftFinder(TEXT("/OpenRTSCamera/Inputs/TurnCameraLeft"));
	static ConstructorHelpers::FObjectFinder<UInputAction>
		TurnCameraRightFinder(TEXT("/OpenRTSCamera/Inputs/TurnCameraRight"));
	static ConstructorHelpers::FObjectFinder<UInputAction>
		ZoomCameraFinder(TEXT("/OpenRTSCamera/Inputs/ZoomCamera"));
	static ConstructorHelpers::FObjectFinder<UInputMappingContext>
		InputMappingContextFinder(TEXT("/OpenRTSCamera/Inputs/OpenRTSCameraInputs"));

	this->MoveCameraXAxis = MoveCameraXAxisFinder.Object;
	this->MoveCameraYAxis = MoveCameraYAxisFinder.Object;
	this->RotateCameraAxis = RotateCameraAxisFinder.Object;
	this->TurnCameraLeft = TurnCameraLeftFinder.Object;
	this->TurnCameraRight = TurnCameraRightFinder.Object;
	this->ZoomCamera = ZoomCameraFinder.Object;
	this->InputMappingContext = InputMappingContextFinder.Object;
}

void URTSCamera::BeginPlay()
{
	Super::BeginPlay();
	// 获得组件引用
	this->CollectComponentDependencyReferences();
	// 初始化弹簧臂
	this->ConfigureSpringArm();
	// 视图找到边界体积的引用
	this->TryToFindBoundaryVolumeReference();
	// 检查是否启用边缘滚动
	this->ConditionallyEnableEdgeScrolling();
	// 检查是否设置了输入
	this->CheckForEnhancedInputComponent();
	// 绑定输入
	this->BindInputMappingContext();
	this->BindInputActions();
	// 设置活动相机
	this->SetActiveCamera();
}

void URTSCamera::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction
)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	this->DeltaSeconds = DeltaTime;
	this->ApplyMoveCameraCommands();
	this->ConditionallyPerformEdgeScrolling();
	this->ConditionallyKeepCameraAtDesiredZoomAboveGround();
	this->SmoothTargetArmLengthToDesiredZoom();
	this->FollowTargetIfSet();
	this->ConditionallyApplyCameraBounds();
}

void URTSCamera::FollowTarget(AActor* Target)
{
	this->CameraFollowTarget = Target;
}

void URTSCamera::UnFollowTarget()
{
	this->CameraFollowTarget = nullptr;
}

void URTSCamera::OnZoomCamera(const FInputActionValue& Value)
{
	/**
	 * 正在缩放摄像机时
	 * 设置长度
	 * 同时始终把长度限制在最大和最小值之间
	 */
	this->DesiredZoomLength = FMath::Clamp(
		this->DesiredZoomLength + Value.Get<float>() * this->ZoomSpeed,
		this->MinimumZoomLength,
		this->MaximumZoomLength
	);
}

void URTSCamera::OnRotateCamera(const FInputActionValue& Value)
{
	/**
	 * 正在旋转摄像机时
	 * 先获取根组件的旋转
	 * 在此基础上增加旋转Z值
	 */
	const auto WorldRotation = this->Root->GetComponentRotation();
	this->Root->SetWorldRotation(
		FRotator::MakeFromEuler(
			FVector(
				WorldRotation.Euler().X,
				WorldRotation.Euler().Y,
				WorldRotation.Euler().Z + Value.Get<float>()
			)
		)
	);
}

void URTSCamera::OnTurnCameraLeft(const FInputActionValue&)
{
	/**
	 * 向左旋转摄像机时
	 * 先获得根组件相对旋转
	 * 在此基础上减旋转Z值
	 */
	const auto WorldRotation = this->Root->GetRelativeRotation();
	this->Root->SetRelativeRotation(
		FRotator::MakeFromEuler(
			FVector(
				WorldRotation.Euler().X,
				WorldRotation.Euler().Y,
				WorldRotation.Euler().Z - this->RotateSpeed
			)
		)
	);
}

void URTSCamera::OnTurnCameraRight(const FInputActionValue&)
{
	/**
	 * 向右旋转摄像机时
	 * 先获得根组件相对旋转
	 * 在此基础上加旋转Z值
	 */
	const auto WorldRotation = this->Root->GetRelativeRotation();
	this->Root->SetRelativeRotation(
		FRotator::MakeFromEuler(
			FVector(
				WorldRotation.Euler().X,
				WorldRotation.Euler().Y,
				WorldRotation.Euler().Z + this->RotateSpeed
			)
		)
	);
}

void URTSCamera::OnMoveCameraYAxis(const FInputActionValue& Value)
{
	/**
	 * 在Y方向移动摄像机
	 * 传入弹簧臂的前向量
	 * 请求移动摄像机
	 */
	this->RequestMoveCamera(
		this->SpringArm->GetForwardVector().X,
		this->SpringArm->GetForwardVector().Y,
		Value.Get<float>()
	);
}

void URTSCamera::OnMoveCameraXAxis(const FInputActionValue& Value)
{
	/**
	 * 在X方向移动摄像机
	 * 传入弹簧臂的右向量
	 * 请求移动摄像机
	 */
	this->RequestMoveCamera(
		this->SpringArm->GetRightVector().X,
		this->SpringArm->GetRightVector().Y,
		Value.Get<float>()
	);
}

void URTSCamera::OnDragCamera(const FInputActionValue& Value)
{
	if (!this->IsDragging && Value.Get<bool>())
	{
		/**
		 * 开始拖动时，需要记录开始拖动的位置，即当前鼠标在视口的位置
		 */
		this->IsDragging = true;
		this->DragStartLocation = UWidgetLayoutLibrary::GetMousePositionOnViewport(this->GetWorld());
	}
	
	else if (this->IsDragging && Value.Get<bool>())
	{
		const auto MousePosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(this->GetWorld());
		/**
		 * auto = FVector2D
		 * 获得视口的尺寸
		 * Gets the geometry of the widget holding all widgets added to the "Viewport". You can use this geometry to convert between absolute and local space of widgets held on this widget.	
		 * DragExtents = ViewportExtents 见DragExtent的注释
		 * 乘DragExtent控制拖动相机的速率
		 */
		auto DragExtents = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this->GetWorld()).GetLocalSize();
		DragExtents *= DragExtent;
		
		auto Delta = MousePosition - this->DragStartLocation;
		Delta.X = FMath::Clamp(Delta.X, -DragExtents.X, DragExtents.X) / DragExtents.X;
		Delta.Y = FMath::Clamp(Delta.Y, -DragExtents.Y, DragExtents.Y) / DragExtents.Y;
		
		/**
		 * 同时请求摄像机在X和Y方向的移动
		 */
		this->RequestMoveCamera(
			this->SpringArm->GetRightVector().X,
			this->SpringArm->GetRightVector().Y,
			Delta.X
		);
		
		this->RequestMoveCamera(
			this->SpringArm->GetForwardVector().X,
			this->SpringArm->GetForwardVector().Y,
			Delta.Y * -1
		);
	}

	else if (this->IsDragging && !Value.Get<bool>())
	{
		this->IsDragging = false;
	}
}

void URTSCamera::RequestMoveCamera(const float X, const float Y, const float Scale)
{
	FMoveCameraCommand MoveCameraCommand;
	MoveCameraCommand.X = X;
	MoveCameraCommand.Y = Y;
	MoveCameraCommand.Scale = Scale;
	MoveCameraCommands.Push(MoveCameraCommand);
}

void URTSCamera::ApplyMoveCameraCommands()
{
	/**
	 * 执行每一个移动指令
	 */
	for (const auto& [X, Y, Scale] : this->MoveCameraCommands)
	{
		auto Movement = FVector2D(X, Y);
		Movement.Normalize();
		Movement *= this->MoveSpeed * Scale * this->DeltaSeconds;
		this->Root->SetWorldLocation(
			this->Root->GetComponentLocation() + FVector(Movement.X, Movement.Y, 0.0f)
		);
	}

	/**
	 * 清空
	 */
	this->MoveCameraCommands.Empty();
}

void URTSCamera::CollectComponentDependencyReferences()
{
	/**
	 * Owner 组件拥有者actor
	 * Root 根组件
	 * Camera 摄像机
	 * SpringArm 弹簧臂
	 * PlayerController 玩家控制器0
	 */
	this->Owner = this->GetOwner();
	this->Root = this->Owner->GetRootComponent();
	this->Camera = Cast<UCameraComponent>(this->Owner->GetComponentByClass(UCameraComponent::StaticClass()));
	this->SpringArm = Cast<USpringArmComponent>(this->Owner->GetComponentByClass(USpringArmComponent::StaticClass()));
	this->PlayerController = UGameplayStatics::GetPlayerController(this->GetWorld(), 0);
}

void URTSCamera::ConfigureSpringArm()
{
	this->DesiredZoomLength = this->MaximumZoomLength;
	this->SpringArm->TargetArmLength = this->DesiredZoomLength;
	this->SpringArm->bDoCollisionTest = false;
	this->SpringArm->bEnableCameraLag = this->EnableCameraLag;
	this->SpringArm->bEnableCameraRotationLag = this->EnableCameraRotationLag;
	this->SpringArm->SetRelativeRotation(
		FRotator::MakeFromEuler(
			FVector(
				0.0,
				this->StartingYAngle,
				this->StartingZAngle
			)
		)
	);
}

void URTSCamera::TryToFindBoundaryVolumeReference()
{
	/**
	 * 从游戏标签寻找边界体积
	 */
	TArray<AActor*> BlockingVolumes;
	UGameplayStatics::GetAllActorsOfClassWithTag(
		this->GetWorld(),
		AActor::StaticClass(),
		this->CameraBlockingVolumeTag,
		BlockingVolumes
	);
	/**
	 * 找到了，数组长度 > 0
	 */
	if (BlockingVolumes.Num() > 0)
	{
		this->BoundaryVolume = BlockingVolumes[0];
	}
}

void URTSCamera::ConditionallyEnableEdgeScrolling() const
{
	/**
	 * Data structure used to setup an input mode that allows the UI to respond to user input, and if the UI doesn't handle it player input / player controller gets a chance.
	 */
	if (this->EnableEdgeScrolling)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
		InputMode.SetHideCursorDuringCapture(false);
		this->PlayerController->SetInputMode(InputMode);
	}
}

void URTSCamera::CheckForEnhancedInputComponent() const
{
	/**
	 * 如果没有启用EnhancedInput需要弹出警告
	 */
	if (Cast<UEnhancedInputComponent>(this->PlayerController->InputComponent) == nullptr)
	{
		UKismetSystemLibrary::PrintString(
			this->GetWorld(),
			TEXT("Set Edit > Project Settings > Input > Default Classes to Enhanced Input Classes"), true, true,
			FLinearColor::Red,
			100
		);

		UKismetSystemLibrary::PrintString(
			this->GetWorld(),
			TEXT("Keyboard inputs will probably not function."), true, true,
			FLinearColor::Red,
			100
		);

		UKismetSystemLibrary::PrintString(
			this->GetWorld(),
			TEXT("Error: Enhanced input component not found."), true, true,
			FLinearColor::Red,
			100
		);
	}
}

void URTSCamera::BindInputMappingContext() const
{
	/**
	 * 显示鼠标指针
	 */
	this->PlayerController->bShowMouseCursor = true;
	/**
	 * 将EnhancedInput添加进键位映射中
	 */
	const auto Subsystem = this
	                       ->PlayerController
	                       ->GetLocalPlayer()
	                       ->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	Subsystem->ClearAllMappings();
	Subsystem->AddMappingContext(this->InputMappingContext, 0);
}

void URTSCamera::BindInputActions()
{
	/**
	 * 绑定输入到函数
	 */
	if (const auto EnhancedInputComponent = Cast<UEnhancedInputComponent>(this->PlayerController->InputComponent))
	{
		EnhancedInputComponent->BindAction(
			this->ZoomCamera,
			ETriggerEvent::Triggered,
			this,
			&URTSCamera::OnZoomCamera
		);

		EnhancedInputComponent->BindAction(
			this->RotateCameraAxis,
			ETriggerEvent::Triggered,
			this,
			&URTSCamera::OnRotateCamera
		);

		EnhancedInputComponent->BindAction(
			this->TurnCameraLeft,
			ETriggerEvent::Triggered,
			this,
			&URTSCamera::OnTurnCameraLeft
		);

		EnhancedInputComponent->BindAction(
			this->TurnCameraRight,
			ETriggerEvent::Triggered,
			this,
			&URTSCamera::OnTurnCameraRight
		);

		EnhancedInputComponent->BindAction(
			this->MoveCameraXAxis,
			ETriggerEvent::Triggered,
			this,
			&URTSCamera::OnMoveCameraXAxis
		);

		EnhancedInputComponent->BindAction(
			this->MoveCameraYAxis,
			ETriggerEvent::Triggered,
			this,
			&URTSCamera::OnMoveCameraYAxis
		);
		
		EnhancedInputComponent->BindAction(
			this->DragCamera,
			ETriggerEvent::Triggered,
			this,
			&URTSCamera::OnDragCamera
		);
	}
}

void URTSCamera::SetActiveCamera() const
{
	UGameplayStatics::GetPlayerController(this->GetWorld(), 0)->SetViewTarget(this->GetOwner());
}

void URTSCamera::ConditionallyPerformEdgeScrolling() const
{
	if (this->EnableEdgeScrolling && !this->IsDragging)
	{
		this->EdgeScrollLeft();
		this->EdgeScrollRight();
		this->EdgeScrollUp();
		this->EdgeScrollDown();
	}
}

void URTSCamera::EdgeScrollLeft() const
{
	const auto MousePosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(this->GetWorld());
	const auto ViewportSize = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this->GetWorld()).GetLocalSize();
	/**
 	 * NormalizeToRange:
	 * Returns Value normalized to the given range. (e.g. 20 normalized to the range 10->50 would result in 0.25)
	 */
	const auto NormalizedMousePosition = 1 - UKismetMathLibrary::NormalizeToRange(
		MousePosition.X,
		0.0f,
		ViewportSize.X * 0.05f
	);

	const auto Movement = UKismetMathLibrary::FClamp(NormalizedMousePosition, 0.0, 1.0);

	this->Root->AddRelativeLocation(
		-1 * this->Root->GetRightVector() * Movement * this->EdgeScrollSpeed * this->DeltaSeconds
	);
}

void URTSCamera::EdgeScrollRight() const
{
	const auto MousePosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(this->GetWorld());
	const auto ViewportSize = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this->GetWorld()).GetLocalSize();
	const auto NormalizedMousePosition = UKismetMathLibrary::NormalizeToRange(
		MousePosition.X,
		ViewportSize.X * 0.95f,
		ViewportSize.X
	);

	const auto Movement = UKismetMathLibrary::FClamp(NormalizedMousePosition, 0.0, 1.0);
	this->Root->AddRelativeLocation(
		this->Root->GetRightVector() * Movement * this->EdgeScrollSpeed * this->DeltaSeconds
	);
}

void URTSCamera::EdgeScrollUp() const
{
	const auto MousePosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(this->GetWorld());
	const auto ViewportSize = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this->GetWorld()).GetLocalSize();
	const auto NormalizedMousePosition = UKismetMathLibrary::NormalizeToRange(
		MousePosition.Y,
		0.0f,
		ViewportSize.Y * 0.05f
	);

	const auto Movement = 1 - UKismetMathLibrary::FClamp(NormalizedMousePosition, 0.0, 1.0);
	this->Root->AddRelativeLocation(
		this->Root->GetForwardVector() * Movement * this->EdgeScrollSpeed * this->DeltaSeconds
	);
}

void URTSCamera::EdgeScrollDown() const
{
	const auto MousePosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(this->GetWorld());
	const auto ViewportSize = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this->GetWorld()).GetLocalSize();
	const auto NormalizedMousePosition = UKismetMathLibrary::NormalizeToRange(
		MousePosition.Y,
		ViewportSize.Y * 0.95f,
		ViewportSize.Y
	);

	const auto Movement = UKismetMathLibrary::FClamp(NormalizedMousePosition, 0.0, 1.0);
	this->Root->AddRelativeLocation(
		-1 * this->Root->GetForwardVector() * Movement * this->EdgeScrollSpeed * this->DeltaSeconds
	);
}

void URTSCamera::FollowTargetIfSet() const
{
	/**
	 * 如果需要跟随目标，则将根组件的位置设置成目标的位置
	 */
	if (this->CameraFollowTarget != nullptr)
	{
		this->Root->SetWorldLocation(this->CameraFollowTarget->GetActorLocation());
	}
}

void URTSCamera::SmoothTargetArmLengthToDesiredZoom() const
{
	/**
	 * 通过改变弹簧臂（插值）的长度实现平滑缩放
	 */
	this->SpringArm->TargetArmLength = FMath::FInterpTo(
		this->SpringArm->TargetArmLength,
		this->DesiredZoomLength,
		this->DeltaSeconds,
		this->ZoomCatchupSpeed
	);
}

void URTSCamera::ConditionallyKeepCameraAtDesiredZoomAboveGround()
{
	if (this->EnableDynamicCameraHeight)
	{
		const auto RootWorldLocation = this->Root->GetComponentLocation();
		const TArray<AActor*> ActorsToIgnore;

		auto HitResult = FHitResult();
		/**
		 * 从很高很高的地方竖直向很低很低的地方发射线
		 // TODO: 遇到多层楼是不是会有BUG？能不能从root开始向上发射线，遇到的第一个阻挡即为最高点？
		 */
		auto DidHit = UKismetSystemLibrary::LineTraceSingle(
			this->GetWorld(),
			FVector(RootWorldLocation.X, RootWorldLocation.Y, RootWorldLocation.Z + this->FindGroundTraceLength),
			FVector(RootWorldLocation.X, RootWorldLocation.Y, RootWorldLocation.Z - this->FindGroundTraceLength),
			UEngineTypes::ConvertToTraceType(this->CollisionChannel),
			true,
			ActorsToIgnore,
			EDrawDebugTrace::Type::None,
			HitResult,
			true,
		);
		/**
		 * 如果有击中，则那个位置是当前场景的最高点，动态调整相机到那个高度
		 */
		if (DidHit)
		{
			this->Root->SetWorldLocation(
				FVector(
					HitResult.Location.X,
					HitResult.Location.Y,
					HitResult.Location.Z
				)
			);
		}

		else if (!this->IsCameraOutOfBoundsErrorAlreadyDisplayed)
		{
			this->IsCameraOutOfBoundsErrorAlreadyDisplayed = true;
			
			UKismetSystemLibrary::PrintString(
				this->GetWorld(),
				"Or add a `RTSCameraBoundsVolume` actor to the scene.",
				true,
				true,
				FLinearColor::Red,
				100
			);
			
			UKismetSystemLibrary::PrintString(
				this->GetWorld(),
				"Increase trace length or change the starting position of the parent actor for the spring arm.",
				true,
				true,
				FLinearColor::Red,
				100
			);
			
			UKismetSystemLibrary::PrintString(
				this->GetWorld(),
				"Error: AC_RTSCamera needs to be placed on the ground!",
				true,
				true,
				FLinearColor::Red,
				100
			);
		}
	}
}

void URTSCamera::ConditionallyApplyCameraBounds() const
{
	if (this->BoundaryVolume != nullptr)
	{
		const auto RootWorldLocation = this->Root->GetComponentLocation();
		FVector Origin;
		FVector Extents;
		/**
		 * GetActorBounds()
		 * Returns the bounding box of all components that make up this Actor (excluding ChildActorComponents).
		 * Output1: Origin: center of the actor in world space
		 * Output2: Extents: half the actor's size in 3d space
		 */
		this->BoundaryVolume->GetActorBounds(false, Origin, Extents);
		/**
		 * 重新调整root的位置，如果超出了边界就会被限制回边界
		 */
		this->Root->SetWorldLocation(
			FVector(
				UKismetMathLibrary::Clamp(RootWorldLocation.X, Origin.X - Extents.X, Origin.X + Extents.X),
				UKismetMathLibrary::Clamp(RootWorldLocation.Y, Origin.Y - Extents.Y, Origin.Y + Extents.Y),
				RootWorldLocation.Z
			)
		);
	}
}
