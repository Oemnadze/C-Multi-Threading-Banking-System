#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <inttypes.h>

#include "teller.h"
#include "account.h"
#include "error.h"
#include "debug.h"
#include "branch.h"

/*
 * deposit money into an account
 */
int
Teller_DoDeposit(Bank *bank, AccountNumber accountNum, AccountAmount amount)
{
  assert(amount >= 0);

  DPRINTF('t', ("Teller_DoDeposit(account 0x%"PRIx64" amount %"PRId64")\n",
                accountNum, amount));

  Account *account = Account_LookupByNumber(bank, accountNum);

  if (account == NULL) {
    return ERROR_ACCOUNT_NOT_FOUND;
  }

  BranchID branchId = AccountNum_GetBranchID(accountNum);

  sem_wait(&(account->accountLocker));
  sem_wait(&(bank->branches[branchId].branchLocker));

  Account_Adjust(bank, account, amount, 1);

  sem_post(&(account->accountLocker));
  sem_post(&(bank->branches[branchId].branchLocker));

  return ERROR_SUCCESS;
}

/*
 * withdraw money from an account
 */
int
Teller_DoWithdraw(Bank *bank, AccountNumber accountNum, AccountAmount amount)
{
  assert(amount >= 0);

  DPRINTF('t', ("Teller_DoWithdraw(account 0x%"PRIx64" amount %"PRId64")\n",
                accountNum, amount));

  Account *account = Account_LookupByNumber(bank, accountNum);

  if (account == NULL) {
    return ERROR_ACCOUNT_NOT_FOUND;
  }

  BranchID branchId = AccountNum_GetBranchID(accountNum);

  sem_wait(&(account->accountLocker));
  sem_wait(&(bank->branches[branchId].branchLocker));

  if (amount > Account_Balance(account)) {
    sem_post(&(account->accountLocker));
    sem_post(&(bank->branches[branchId].branchLocker));
    return ERROR_INSUFFICIENT_FUNDS;
  }

  Account_Adjust(bank,account, -amount, 1);

  sem_post(&(account->accountLocker));
  sem_post(&(bank->branches[branchId].branchLocker));

  return ERROR_SUCCESS;
}

/*
 * do a tranfer from one account to another account
 */
int
Teller_DoTransfer(Bank *bank, AccountNumber srcAccountNum,
                  AccountNumber dstAccountNum,
                  AccountAmount amount)
{
  assert(amount >= 0);

  DPRINTF('t', ("Teller_DoTransfer(src 0x%"PRIx64", dst 0x%"PRIx64
                ", amount %"PRId64")\n",
                srcAccountNum, dstAccountNum, amount));

  Account *srcAccount = Account_LookupByNumber(bank, srcAccountNum);
  if (srcAccount == NULL) {
    return ERROR_ACCOUNT_NOT_FOUND;
  }

  Account *dstAccount = Account_LookupByNumber(bank, dstAccountNum);
  if (dstAccount == NULL) {
    return ERROR_ACCOUNT_NOT_FOUND;
  }

  if (srcAccountNum == dstAccountNum)
    return ERROR_SUCCESS;

  if (amount > Account_Balance(srcAccount)) {
    return ERROR_INSUFFICIENT_FUNDS;
  }

  BranchID id1 = AccountNum_GetBranchID(srcAccountNum);
  BranchID id2 = AccountNum_GetBranchID(dstAccountNum);

  if (srcAccountNum >= dstAccountNum) {
    sem_wait(&(dstAccount->accountLocker));
    sem_wait(&(srcAccount->accountLocker));
  }
  else {
    sem_wait(&(srcAccount->accountLocker));
    sem_wait(&(dstAccount->accountLocker));
  }

  /*
   * If we are doing a transfer within the branch, we tell the Account module to
   * not bother updating the branch balance since the net change for the
   * branch is 0.
   */
  int updateBranch = !Account_IsSameBranch(srcAccountNum, dstAccountNum);

  if (updateBranch) {
    if (srcAccountNum >= dstAccountNum) {
      sem_wait(&(bank->branches[id2].branchLocker));
      sem_wait(&(bank->branches[id1].branchLocker));
    }
    else {
      sem_wait(&(bank->branches[id1].branchLocker));
      sem_wait(&(bank->branches[id2].branchLocker));
    }
  }

  Account_Adjust(bank, srcAccount, -amount, updateBranch);
  Account_Adjust(bank, dstAccount, amount, updateBranch);

  if (updateBranch) {
    sem_post(&(bank->branches[id1].branchLocker));
    sem_post(&(bank->branches[id2].branchLocker));
  }

  sem_post(&(srcAccount->accountLocker));
  sem_post(&(dstAccount->accountLocker));

  return ERROR_SUCCESS;
  
}
