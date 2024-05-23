// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/Cpp_AC_Inventory.h"
#include "ItemBase.h"

// Constructor for the class.
UCpp_AC_Inventory::UCpp_AC_Inventory() {

	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	
}

void UCpp_AC_Inventory::BeginPlay() {
	Super::BeginPlay();


}

void UCpp_AC_Inventory::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

}



UItemBase* UCpp_AC_Inventory::FindMatchingItem(UItemBase* InItem) const {
	if(InItem && InventoryContents.Contains(InItem)) {
		return InItem;		
	}
	return nullptr;
}

UItemBase* UCpp_AC_Inventory::FindNextItemByID(UItemBase* InItem) const {
	// Only works as overloaded operator is implemented in UItemBase for the == operator.
	// Element Type is used to get the type of the array elements.
	if(InItem) {
		const TArray<TObjectPtr<UItemBase>>::ElementType* Result = InventoryContents.FindByKey(InItem);
		if (Result)
			return *Result;
	}
	return nullptr;
}

UItemBase* UCpp_AC_Inventory::FindNextPartialStack(UItemBase* InItem) const {
	/*Summary Of If Statement :
	Result is a pointer to the element type of the array UItemBase.
	FindByPredicate is used to find the first element that satisfies the predicate.
	FindByPredicate returns a pointer to the element type of the array UItemBase.
	Lambda function is used to check if the ID of both are the same AND
	if the item is not a full stack which means it can be added to
	[&InItem] is the capture list for the lambda function which
	means it can access the InItem variable to compare it with the InventoryItem.
	If the result is not null, return the result */
	if(const TArray<TObjectPtr<UItemBase>>::ElementType* Result = 
		InventoryContents.FindByPredicate([&InItem](const UItemBase* InventoryItem) {
				return InventoryItem->ID == InItem->ID && !InventoryItem->IsFullItemStack();
			}
		)) {
		return *Result;
	}
	return nullptr;
}

void UCpp_AC_Inventory::RemoveSingleInstanceOfItem(UItemBase* ItemToRemove) {
	InventoryContents.RemoveSingle(ItemToRemove);
	// Calls The Broadcast Function To Tell Other Classes That The Inventory Has Been Updated.
	OnInventoryUpdated.Broadcast();
}

int32 UCpp_AC_Inventory::RemoveAmountOfItem(UItemBase* InItem, const int32 AmountToRemove) {
	const int32 ActualAmountToRemove = FMath::Min(AmountToRemove, InItem->Quantity);
	InItem->SetQuantity(InItem->Quantity - ActualAmountToRemove);

	// Reduces the total weight of the inventory by the amount of items removed and their weight.
	InventoryTotalWeight -= ActualAmountToRemove * InItem->GetItemSingleWeight();

	// Calls The Broadcast Function To Tell Other Classes That The Inventory Has Been Updated.
	OnInventoryUpdated.Broadcast();
	return ActualAmountToRemove;
}

void UCpp_AC_Inventory::SplitExistingStack(UItemBase* InItem, const int32 AmountToSplit) {
	if(!(InventoryContents.Num() + 1 > InventorySlotsCapacity)) {
		RemoveAmountOfItem(InItem, AmountToSplit);
		AddNewItem(InItem, AmountToSplit);
	}
}

int32 UCpp_AC_Inventory::CalculateWeightAddAmount(UItemBase* InItem, int32 AddAmount) {
	// Calculate the amount of weight that can be added to the inventory.
	// FloorToInt is used to round down the result of the division
	// eg. (90 - 55) / 10 = 3.5 -> FloorToInt(3.5) = 3
	const int32 WeightMaxAddAmount = FMath::FloorToInt((GetWeightCapacity() - InventoryTotalWeight) 
													   / InItem->GetItemSingleWeight());
	if(WeightMaxAddAmount >= AddAmount) {
		return AddAmount;
	}
	return WeightMaxAddAmount;
}

int32 UCpp_AC_Inventory::CalculateNumberForFullStack(UItemBase* StackableItem, int32 InitialAddAmount) {
	const int32 AddAmountToMakeFullStack = StackableItem->ItemNumericData.MaxStackSize - StackableItem->Quantity;
	return FMath::Min(InitialAddAmount, AddAmountToMakeFullStack);
}

FItemAddResult UCpp_AC_Inventory::HandleNonStackableItems(UItemBase* InItem) {
	// Check if input item has valid weight
	if(FMath::IsNearlyZero(InItem->GetItemSingleWeight()) || InItem->GetItemSingleWeight() < 0) {
		// return added no items
		return FItemAddResult::AddedNone(FText::Format(
				FText::FromString("Could not add {0} to the inventory. Item Has No Weight!"), 
					InItem->ItemTextData.ItemName));
	}

	// Will item weight exceed inventory weight capacity?
	if(InventoryTotalWeight + InItem->GetItemSingleWeight() > GetWeightCapacity()) {
		// return added no items
		return FItemAddResult::AddedNone(FText::Format(
			FText::FromString("Could not add {0} to the inventory. Item overflows weight limit!"),
			InItem->ItemTextData.ItemName));
	}

	// Will inventory exceed capacity?
	if(InventoryContents.Num() + 1 > InventorySlotsCapacity) {
		return FItemAddResult::AddedNone(FText::Format(
			FText::FromString("Could not add {0} to the inventory. No Free Inventory Slot!"),
			InItem->ItemTextData.ItemName));
	}

	AddNewItem(InItem, 1);
	// return added all items
	return FItemAddResult::AddedAll(1, FText::Format(
		FText::FromString("Successfully added {0} to the inventory!"),
		InItem->ItemTextData.ItemName));
}

int32 UCpp_AC_Inventory::HandleStackableItems(UItemBase* InItem, int32 AddAmount) {

	return 0;
}

FItemAddResult UCpp_AC_Inventory::HandleAddItem(UItemBase* InItem) {
	if(GetOwner()) {
		const int32 InitialRequestedAddAmount = InItem->Quantity;

		// Handle Non-Stackable Items
		if(!InItem->ItemNumericData.bIsStackable) {
			return HandleNonStackableItems(InItem);
		}

		// Handle Stackable Items
		const int32 StackableAmountAdded = HandleStackableItems(InItem, InitialRequestedAddAmount);

		if(StackableAmountAdded == InitialRequestedAddAmount) {
			return FItemAddResult::AddedAll(InitialRequestedAddAmount, FText::Format(
				FText::FromString("Successfully added {0} {1} to the inventory!"),
				InItem->ItemTextData.ItemName, StackableAmountAdded));
		}
		else if(StackableAmountAdded < InitialRequestedAddAmount && StackableAmountAdded > 0) {
			return FItemAddResult::AddedSome(StackableAmountAdded, FText::Format(
				FText::FromString("Could not add all {0} to the inventory. Added {1} {0} instead!"),
				InItem->ItemTextData.ItemName, StackableAmountAdded));
		}
		else {
			return FItemAddResult::AddedNone(FText::Format(
				FText::FromString("Could not add {0} to the inventory. No Remaining Slots / Invalid Item!"),
				InItem->ItemTextData.ItemName));
		}
	}
	return FItemAddResult::AddedNone(FText::FromString("Could not add item to the inventory. No Owner Found!"));
}

void UCpp_AC_Inventory::AddNewItem(UItemBase* InItem, const int32 AddAmount) {
	UItemBase* NewItem;

	if(InItem->bIsCopy || InItem->bIsPickup) {
		// If the item is already a copy or a world pickup
		NewItem = InItem;
		NewItem->ResetItemFlags();
	}
	else {
		// used for splitting or dragging items from another inventory
		NewItem = InItem->CreateItemCopy();
	}
	NewItem->OwningInventory = this;
	NewItem->SetQuantity(AddAmount);

	InventoryContents.Add(NewItem);
	InventoryTotalWeight += NewItem->GetItemStackWeight();	
	OnInventoryUpdated.Broadcast();
}

